#include "network/TlsSession.h"

#if defined(ENGINE_ENABLE_TLS)

#include "Logger.h"

#include <cstring>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <psa/crypto.h>

#include <mutex>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

using EngineLogging::Logger;

namespace Engine {
namespace Platform {
namespace Networking {

struct TlsSession::Impl {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctrDrbg;
    mbedtls_x509_crt caChain;
    mbedtls_x509_crt serverCert;
    mbedtls_pk_context privateKey;
    uintptr_t socketHandle = 0;
};

namespace {

void EnsurePsaCryptoInitialized() {
    static std::once_flag initialized;
    std::call_once(initialized, []() { psa_crypto_init(); });
}

int BioSend(void* ctx, const unsigned char* buf, size_t len) {
    auto* socketHandle = static_cast<uintptr_t*>(ctx);
#ifdef _WIN32
    const int sent = ::send(static_cast<SOCKET>(*socketHandle), reinterpret_cast<const char*>(buf), static_cast<int>(len), 0);
#else
    const ssize_t sent = ::send(static_cast<int>(*socketHandle), buf, len, 0);
#endif
    if (sent < 0) {
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }
    return static_cast<int>(sent);
}

int BioRecv(void* ctx, unsigned char* buf, size_t len) {
    auto* socketHandle = static_cast<uintptr_t*>(ctx);
#ifdef _WIN32
    const int received =
        ::recv(static_cast<SOCKET>(*socketHandle), reinterpret_cast<char*>(buf), static_cast<int>(len), 0);
#else
    const ssize_t received = ::recv(static_cast<int>(*socketHandle), buf, len, 0);
#endif
    if (received == 0) {
        return MBEDTLS_ERR_NET_CONN_RESET;
    }
    if (received < 0) {
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    return static_cast<int>(received);
}

} // namespace

TlsSession::TlsSession() = default;

TlsSession::~TlsSession() {
    Close();
}

bool TlsSession::InitializeClient(uintptr_t socketHandle, const TlsConfig& config) {
    Close();
    if (!config.enabled) {
        return false;
    }

    EnsurePsaCryptoInitialized();

    m_impl = new Impl();
    mbedtls_ssl_init(&m_impl->ssl);
    mbedtls_ssl_config_init(&m_impl->conf);
    mbedtls_entropy_init(&m_impl->entropy);
    mbedtls_ctr_drbg_init(&m_impl->ctrDrbg);
    mbedtls_x509_crt_init(&m_impl->caChain);

    const char* pers = "renderingengine-client";
    if (mbedtls_ctr_drbg_seed(&m_impl->ctrDrbg, mbedtls_entropy_func, &m_impl->entropy,
                              reinterpret_cast<const unsigned char*>(pers), std::strlen(pers)) != 0) {
        Close();
        return false;
    }

    if (mbedtls_ssl_config_defaults(&m_impl->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        Close();
        return false;
    }

    mbedtls_ssl_conf_authmode(&m_impl->conf,
                              config.insecureSkipVerify ? MBEDTLS_SSL_VERIFY_NONE : MBEDTLS_SSL_VERIFY_REQUIRED);

    if (!config.caCertificatePath.empty()) {
        if (mbedtls_x509_crt_parse_file(&m_impl->caChain, config.caCertificatePath.c_str()) != 0) {
            Close();
            return false;
        }
        mbedtls_ssl_conf_ca_chain(&m_impl->conf, &m_impl->caChain, nullptr);
    } else if (!config.insecureSkipVerify) {
        Close();
        return false;
    }

    mbedtls_ssl_conf_rng(&m_impl->conf, mbedtls_ctr_drbg_random, &m_impl->ctrDrbg);
    if (mbedtls_ssl_setup(&m_impl->ssl, &m_impl->conf) != 0) {
        Close();
        return false;
    }

    if (!config.serverName.empty()) {
        if (mbedtls_ssl_set_hostname(&m_impl->ssl, config.serverName.c_str()) != 0) {
            Close();
            return false;
        }
    }

    m_impl->socketHandle = socketHandle;
    mbedtls_ssl_set_bio(&m_impl->ssl, &m_impl->socketHandle, BioSend, BioRecv, nullptr);
    m_active = true;
    return true;
}

bool TlsSession::InitializeServer(uintptr_t socketHandle, const TlsConfig& config) {
    Close();
    if (!config.enabled || config.certificatePath.empty() || config.privateKeyPath.empty()) {
        return false;
    }

    EnsurePsaCryptoInitialized();

    m_impl = new Impl();
    mbedtls_ssl_init(&m_impl->ssl);
    mbedtls_ssl_config_init(&m_impl->conf);
    mbedtls_entropy_init(&m_impl->entropy);
    mbedtls_ctr_drbg_init(&m_impl->ctrDrbg);
    mbedtls_x509_crt_init(&m_impl->serverCert);
    mbedtls_pk_init(&m_impl->privateKey);

    const char* pers = "renderingengine-server";
    if (mbedtls_ctr_drbg_seed(&m_impl->ctrDrbg, mbedtls_entropy_func, &m_impl->entropy,
                              reinterpret_cast<const unsigned char*>(pers), std::strlen(pers)) != 0) {
        Close();
        return false;
    }

    if (mbedtls_ssl_config_defaults(&m_impl->conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        Close();
        return false;
    }

    if (mbedtls_x509_crt_parse_file(&m_impl->serverCert, config.certificatePath.c_str()) != 0 ||
        mbedtls_pk_parse_keyfile(&m_impl->privateKey,
                                 config.privateKeyPath.c_str(),
                                 nullptr,
                                 mbedtls_ctr_drbg_random,
                                 &m_impl->ctrDrbg) != 0) {
        Close();
        return false;
    }

    if (mbedtls_ssl_conf_own_cert(&m_impl->conf, &m_impl->serverCert, &m_impl->privateKey) != 0) {
        Close();
        return false;
    }

    mbedtls_ssl_conf_rng(&m_impl->conf, mbedtls_ctr_drbg_random, &m_impl->ctrDrbg);
    if (mbedtls_ssl_setup(&m_impl->ssl, &m_impl->conf) != 0) {
        Close();
        return false;
    }

    m_impl->socketHandle = socketHandle;
    mbedtls_ssl_set_bio(&m_impl->ssl, &m_impl->socketHandle, BioSend, BioRecv, nullptr);
    m_active = true;
    return true;
}

bool TlsSession::Handshake() {
    if (!m_impl || !m_active) {
        return false;
    }

    int result = 0;
    do {
        result = mbedtls_ssl_handshake(&m_impl->ssl);
    } while (result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (result != 0) {
        char errorBuffer[256];
        mbedtls_strerror(result, errorBuffer, sizeof(errorBuffer));
        LOG_ERROR_FMT("TlsSession", "Handshake failed: -0x%x (%s)", -result, errorBuffer);
        Close();
        return false;
    }

    return true;
}

bool TlsSession::Send(const uint8_t* data, size_t size) {
    if (!m_impl || !m_active) {
        return false;
    }

    size_t offset = 0;
    while (offset < size) {
        const int written =
            mbedtls_ssl_write(&m_impl->ssl, data + offset, static_cast<int>(size - offset));
        if (written <= 0) {
            return false;
        }
        offset += static_cast<size_t>(written);
    }
    return true;
}

bool TlsSession::Recv(uint8_t* data, size_t capacity, size_t& outReceived) {
    if (!m_impl || !m_active) {
        return false;
    }

    const int received = mbedtls_ssl_read(&m_impl->ssl, data, static_cast<int>(capacity));
    if (received == MBEDTLS_ERR_SSL_WANT_READ || received == MBEDTLS_ERR_SSL_WANT_WRITE) {
        outReceived = 0;
        return true;
    }
    if (received <= 0) {
        return false;
    }

    outReceived = static_cast<size_t>(received);
    return true;
}

bool TlsSession::RecvExact(uint8_t* data, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        size_t received = 0;
        if (!Recv(data + offset, size - offset, received)) {
            return false;
        }
        if (received == 0) {
            return false;
        }
        offset += received;
    }
    return true;
}

void TlsSession::Close() {
    if (!m_impl) {
        m_active = false;
        return;
    }

    mbedtls_ssl_free(&m_impl->ssl);
    mbedtls_ssl_config_free(&m_impl->conf);
    mbedtls_entropy_free(&m_impl->entropy);
    mbedtls_ctr_drbg_free(&m_impl->ctrDrbg);
    mbedtls_x509_crt_free(&m_impl->caChain);
    mbedtls_x509_crt_free(&m_impl->serverCert);
    mbedtls_pk_free(&m_impl->privateKey);
    delete m_impl;
    m_impl = nullptr;
    m_active = false;
}

} // namespace Networking
} // namespace Platform
} // namespace Engine

#endif
