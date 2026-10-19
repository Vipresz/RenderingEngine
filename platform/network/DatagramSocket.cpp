#include "network/DatagramSocket.h"

#include "Logger.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using EngineLogging::Logger;

namespace Engine {
namespace Platform {
namespace Networking {

namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidNativeSocket = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidNativeSocket = -1;
#endif

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

DatagramSocket DatagramSocket::CreateUdp() {
    if (!Socket::InitializeNetworking()) {
        return DatagramSocket();
    }
    const NativeSocket handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (handle == kInvalidNativeSocket) {
        return DatagramSocket();
    }
    return DatagramSocket(FromNative(handle));
}

DatagramSocket::DatagramSocket(uintptr_t handle) : m_handle(handle) {}

DatagramSocket::~DatagramSocket() {
    Close();
}

DatagramSocket::DatagramSocket(DatagramSocket&& other) noexcept
    : m_handle(other.m_handle),
      m_connected(other.m_connected),
      m_connectedHost(std::move(other.m_connectedHost)),
      m_connectedPort(other.m_connectedPort) {
    other.m_handle = kInvalidHandle;
    other.m_connected = false;
    other.m_connectedPort = 0;
}

DatagramSocket& DatagramSocket::operator=(DatagramSocket&& other) noexcept {
    if (this != &other) {
        Close();
        m_handle = other.m_handle;
        m_connected = other.m_connected;
        m_connectedHost = std::move(other.m_connectedHost);
        m_connectedPort = other.m_connectedPort;
        other.m_handle = kInvalidHandle;
        other.m_connected = false;
        other.m_connectedPort = 0;
    }
    return *this;
}

void DatagramSocket::Close() {
    if (!IsValid()) {
        return;
    }
#ifdef _WIN32
    shutdown(ToNative(m_handle), SD_BOTH);
    closesocket(ToNative(m_handle));
#else
    const int fd = static_cast<int>(ToNative(m_handle));
    shutdown(fd, SHUT_RDWR);
    close(fd);
#endif
    m_handle = kInvalidHandle;
    m_connected = false;
    m_connectedHost.clear();
    m_connectedPort = 0;
}

bool DatagramSocket::SetNonBlocking(bool enabled) {
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

bool DatagramSocket::SetReuseAddress(bool enabled) {
    if (!IsValid()) {
        return false;
    }
    const int value = enabled ? 1 : 0;
    return SetSocketOptionInt(ToNative(m_handle), SOL_SOCKET, SO_REUSEADDR, value);
}

bool DatagramSocket::Bind(const std::string& host, uint16_t port) {
    if (!IsValid()) {
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
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

bool DatagramSocket::Connect(const std::string& host, uint16_t port) {
    if (!IsValid()) {
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* result = nullptr;
    const std::string portString = std::to_string(port);
    if (getaddrinfo(host.c_str(), portString.c_str(), &hints, &result) != 0) {
        return false;
    }

    bool connected = false;
    for (addrinfo* cursor = result; cursor != nullptr; cursor = cursor->ai_next) {
        if (connect(ToNative(m_handle), cursor->ai_addr, static_cast<int>(cursor->ai_addrlen)) == 0) {
            connected = true;
            m_connectedHost = host;
            m_connectedPort = port;
            break;
        }
    }

    freeaddrinfo(result);
    m_connected = connected;
    return connected;
}

SocketResult DatagramSocket::SendTo(const uint8_t* data, size_t size, const std::string& host, uint16_t port) {
    if (!IsValid() || !data || size == 0) {
        return SocketResult::Error;
    }

    if (m_connected) {
        const int sent = send(ToNative(m_handle), reinterpret_cast<const char*>(data), static_cast<int>(size), 0);
        if (sent < 0) {
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
        return SocketResult::Ok;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* result = nullptr;
    const std::string portString = std::to_string(port);
    if (getaddrinfo(host.c_str(), portString.c_str(), &hints, &result) != 0) {
        return SocketResult::Error;
    }

    SocketResult sendResult = SocketResult::Error;
    for (addrinfo* cursor = result; cursor != nullptr; cursor = cursor->ai_next) {
        const int sent = sendto(ToNative(m_handle),
                                reinterpret_cast<const char*>(data),
                                static_cast<int>(size),
                                0,
                                cursor->ai_addr,
                                static_cast<int>(cursor->ai_addrlen));
        if (sent >= 0) {
            sendResult = SocketResult::Ok;
            break;
        }
    }

    freeaddrinfo(result);
    return sendResult;
}

SocketResult DatagramSocket::RecvFrom(uint8_t* data,
                                      size_t capacity,
                                      size_t& outReceived,
                                      std::string& outHost,
                                      uint16_t& outPort) {
    outReceived = 0;
    outHost.clear();
    outPort = 0;
    if (!IsValid() || !data || capacity == 0) {
        return SocketResult::Error;
    }

    sockaddr_storage addr{};
    socklen_t addrLen = sizeof(addr);
    const int received = recvfrom(ToNative(m_handle),
                                  reinterpret_cast<char*>(data),
                                  static_cast<int>(capacity),
                                  0,
                                  reinterpret_cast<sockaddr*>(&addr),
                                  &addrLen);
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

    outReceived = static_cast<size_t>(received);
    char hostBuffer[NI_MAXHOST] = {};
    char serviceBuffer[NI_MAXSERV] = {};
    if (getnameinfo(reinterpret_cast<sockaddr*>(&addr),
                    addrLen,
                    hostBuffer,
                    sizeof(hostBuffer),
                    serviceBuffer,
                    sizeof(serviceBuffer),
                    NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
        outHost = hostBuffer;
        outPort = static_cast<uint16_t>(std::stoi(serviceBuffer));
    }
    return SocketResult::Ok;
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
