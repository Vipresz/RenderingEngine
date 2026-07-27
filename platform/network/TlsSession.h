#pragma once

#include <string>

namespace Engine {
namespace Platform {
namespace Networking {

struct TlsConfig {
    bool enabled = false;
    std::string certificatePath;
    std::string privateKeyPath;
    std::string caCertificatePath;
    bool insecureSkipVerify = false;
};

#if defined(ENGINE_ENABLE_TLS)

class TlsSession {
public:
    TlsSession();
    ~TlsSession();

    TlsSession(const TlsSession&) = delete;
    TlsSession& operator=(const TlsSession&) = delete;

    bool InitializeClient(uintptr_t socketHandle, const TlsConfig& config);
    bool InitializeServer(uintptr_t socketHandle, const TlsConfig& config);
    bool Handshake();

    bool Send(const uint8_t* data, size_t size);
    bool Recv(uint8_t* data, size_t capacity, size_t& outReceived);
    bool RecvExact(uint8_t* data, size_t size);

    void Close();
    bool IsActive() const { return m_active; }

private:
    struct Impl;
    Impl* m_impl = nullptr;
    bool m_active = false;
};

#else

class TlsSession {
public:
    ~TlsSession() = default;
    bool InitializeClient(uintptr_t, const TlsConfig&) { return false; }
    bool InitializeServer(uintptr_t, const TlsConfig&) { return false; }
    bool Handshake() { return false; }
    bool Send(const uint8_t*, size_t) { return false; }
    bool Recv(uint8_t*, size_t, size_t&) { return false; }
    bool RecvExact(uint8_t*, size_t) { return false; }
    void Close() {}
    bool IsActive() const { return false; }
};

#endif

} // namespace Networking
} // namespace Platform
} // namespace Engine
