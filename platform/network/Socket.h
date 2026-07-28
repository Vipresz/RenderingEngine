#pragma once

#include <cstdint>
#include <string>

namespace Engine {
namespace Platform {
namespace Networking {

enum class SocketResult {
    Ok = 0,
    Error = 1,
    Disconnected = 2,
    WouldBlock = 3,
};

class Socket {
public:
    static bool InitializeNetworking();
    static void ShutdownNetworking();
    static Socket CreateTcp();

    Socket() = default;
    explicit Socket(uintptr_t handle);
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    bool IsValid() const { return m_handle != kInvalidHandle; }
    uintptr_t NativeHandle() const { return m_handle; }

    void Close();

    bool SetNonBlocking(bool enabled);
    bool SetReuseAddress(bool enabled);
    bool SetTcpNoDelay(bool enabled);

    bool Bind(const std::string& host, uint16_t port);
    bool Listen(int backlog = 8);
    bool Connect(const std::string& host, uint16_t port);
    bool Accept(Socket& outClient);

    SocketResult SendAll(const uint8_t* data, size_t size);
    SocketResult RecvAll(uint8_t* data, size_t size);
    SocketResult RecvSome(uint8_t* data, size_t capacity, size_t& outReceived);

private:
    static constexpr uintptr_t kInvalidHandle = static_cast<uintptr_t>(-1);

    uintptr_t m_handle = kInvalidHandle;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
