#pragma once

#include <cstdint>
#include <string>

namespace Engine {
namespace Net {

// Per-shard capacity and redirect metadata for gateway-style routing.
struct ShardConfig {
    uint32_t shardId = 0;
    uint32_t maxPlayers = 128;
    std::string redirectHost;
    uint16_t redirectTcpPort = 0;
    uint16_t redirectUdpPort = 0;
};

} // namespace Net
} // namespace Engine
