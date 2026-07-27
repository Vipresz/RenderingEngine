#pragma once

#include "network/Socket.h"

#include <cstdint>
#include <string>

namespace Engine {
namespace Platform {
namespace Networking {

// UDP socket for unreliable channels (movement, frequent state).
class DatagramSocket {
public:
    static DatagramSocket CreateUdp();

    DatagramSocket() = default;
    explicit DatagramSocket(uintptr_t handle);
    ~DatagramSocket();

    DatagramSocket(const DatagramSocket&) = delete;
    DatagramSocket& operator=(const DatagramSocket&) = delete;

    DatagramSocket(DatagramSocket&& other) noexcept;
    DatagramSocket& operator=(DatagramSocket&& other) noexcept;

    bool IsValid() const { return m_handle != kInvalidHandle; }
    void Close();

    bool SetNonBlocking(bool enabled);
    bool SetReuseAddress(bool enabled);
    bool Bind(const std::string& host, uint16_t port);
    bool Connect(const std::string& host, uint16_t port);

    SocketResult SendTo(const uint8_t* data, size_t size, const std::string& host, uint16_t port);
    SocketResult RecvFrom(uint8_t* data, size_t capacity, size_t& outReceived, std::string& outHost, uint16_t& outPort);

private:
    static constexpr uintptr_t kInvalidHandle = static_cast<uintptr_t>(-1);
    uintptr_t m_handle = kInvalidHandle;
    bool m_connected = false;
    std::string m_connectedHost;
    uint16_t m_connectedPort = 0;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
