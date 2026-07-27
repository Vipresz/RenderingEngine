#include "network/Socket.h"

#include "Logger.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidNativeSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using NativeSocket = int;
constexpr NativeSocket kInvalidNativeSocket = -1;
#endif

using EngineLogging::Logger;

namespace Engine {
namespace Platform {
namespace Networking {

namespace {

NativeSocket ToNative(uintptr_t handle) {
    return static_cast<NativeSocket>(handle);
}

uintptr_t FromNative(NativeSocket handle) {
    return static_cast<uintptr_t>(handle);
}

bool SetSocketOptionInt(NativeSocket socket, int level, int option, int value) {
    return setsockopt(socket, level, option, reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
}

} // namespace

bool Socket::InitializeNetworking() {
#ifdef _WIN32
    WSADATA wsaData{};
    const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        LOG_ERROR_FMT("Socket", "WSAStartup failed: %d", result);
        return false;
    }
#endif
    return true;
}

void Socket::ShutdownNetworking() {
#ifdef _WIN32
    WSACleanup();
#endif
}

Socket::Socket(uintptr_t handle) : m_handle(handle) {}

Socket::~Socket() {
    Close();
}

Socket::Socket(Socket&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = kInvalidHandle;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        Close();
        m_handle = other.m_handle;
        other.m_handle = kInvalidHandle;
    }
    return *this;
}

void Socket::Close() {
    if (!IsValid()) {
        return;
    }
#ifdef _WIN32
    closesocket(ToNative(m_handle));
#else
    close(static_cast<int>(ToNative(m_handle)));
#endif
    m_handle = kInvalidHandle;
}

bool Socket::SetNonBlocking(bool enabled) {
    if (!IsValid()) {
        return false;
    }
#ifdef _WIN32
    u_long mode = enabled ? 1 : 0;
    return ioctlsocket(ToNative(m_handle), FIONBIO, &mode) == 0;
#else
    int flags = fcntl(ToNative(m_handle), F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(ToNative(m_handle), F_SETFL, enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) == 0;
#endif
}

bool Socket::SetReuseAddress(bool enabled) {
    if (!IsValid()) {
        return false;
    }
    const int value = enabled ? 1 : 0;
    return SetSocketOptionInt(ToNative(m_handle), SOL_SOCKET, SO_REUSEADDR, value);
}

bool Socket::SetTcpNoDelay(bool enabled) {
    if (!IsValid()) {
        return false;
    }
    const int value = enabled ? 1 : 0;
    return SetSocketOptionInt(ToNative(m_handle), IPPROTO_TCP, TCP_NODELAY, value);
}

bool Socket::Bind(const std::string& host, uint16_t port) {
    if (!IsValid()) {
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    addrinfo* result = nullptr;
    const std::string portString = std::to_string(port);
    if (getaddrinfo(host.empty() ? nullptr : host.c_str(), portString.c_str(), &hints, &result) != 0) {
        return false;
    }

    bool bound = false;
    for (addrinfo* cursor = result; cursor != nullptr; cursor = cursor->ai_next) {
        if (bind(ToNative(m_handle), cursor->ai_addr, static_cast<int>(cursor->ai_addrlen)) == 0) {
            bound = true;
            break;
        }
    }

    freeaddrinfo(result);
    return bound;
}

bool Socket::Listen(int backlog) {
    if (!IsValid()) {
        return false;
    }
    return listen(ToNative(m_handle), backlog) == 0;
}

bool Socket::Connect(const std::string& host, uint16_t port) {
    if (!IsValid()) {
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    const std::string portString = std::to_string(port);
    if (getaddrinfo(host.c_str(), portString.c_str(), &hints, &result) != 0) {
        return false;
    }

    bool connected = false;
    for (addrinfo* cursor = result; cursor != nullptr; cursor = cursor->ai_next) {
        if (connect(ToNative(m_handle), cursor->ai_addr, static_cast<int>(cursor->ai_addrlen)) == 0) {
            connected = true;
            SetTcpNoDelay(true);
            break;
        }
    }

    freeaddrinfo(result);
    return connected;
}

bool Socket::Accept(Socket& outClient) {
    if (!IsValid()) {
        return false;
    }

    const NativeSocket clientHandle = accept(ToNative(m_handle), nullptr, nullptr);
    if (clientHandle == kInvalidNativeSocket) {
        return false;
    }

    outClient.Close();
    outClient.m_handle = FromNative(clientHandle);
    outClient.SetTcpNoDelay(true);
    return true;
}

SocketResult Socket::SendAll(const uint8_t* data, size_t size) {
    if (!IsValid() || !data || size == 0) {
        return SocketResult::Error;
    }

    size_t sentTotal = 0;
    while (sentTotal < size) {
        const int sent = send(ToNative(m_handle),
                              reinterpret_cast<const char*>(data + sentTotal),
                              static_cast<int>(size - sentTotal),
                              0);
        if (sent <= 0) {
            return sent == 0 ? SocketResult::Disconnected : SocketResult::Error;
        }
        sentTotal += static_cast<size_t>(sent);
    }
    return SocketResult::Ok;
}

SocketResult Socket::RecvAll(uint8_t* data, size_t size) {
    if (!IsValid() || !data || size == 0) {
        return SocketResult::Error;
    }

    size_t receivedTotal = 0;
    while (receivedTotal < size) {
        const int received = recv(ToNative(m_handle),
                                reinterpret_cast<char*>(data + receivedTotal),
                                static_cast<int>(size - receivedTotal),
                                0);
        if (received <= 0) {
            return received == 0 ? SocketResult::Disconnected : SocketResult::Error;
        }
        receivedTotal += static_cast<size_t>(received);
    }
    return SocketResult::Ok;
}

SocketResult Socket::RecvSome(uint8_t* data, size_t capacity, size_t& outReceived) {
    outReceived = 0;
    if (!IsValid() || !data || capacity == 0) {
        return SocketResult::Error;
    }

    const int received = recv(ToNative(m_handle), reinterpret_cast<char*>(data), static_cast<int>(capacity), 0);
    if (received < 0) {
#ifdef _WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return SocketResult::WouldBlock;
        }
#else
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return SocketResult::WouldBlock;
        }
#endif
        return SocketResult::Error;
    }
    if (received == 0) {
        return SocketResult::Disconnected;
    }

    outReceived = static_cast<size_t>(received);
    return SocketResult::Ok;
}

Socket Socket::CreateTcp() {
    const NativeSocket handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (handle == kInvalidNativeSocket) {
        return Socket();
    }
    return Socket(FromNative(handle));
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
