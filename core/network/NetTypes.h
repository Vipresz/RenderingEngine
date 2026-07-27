#pragma once

#include <cstdint>

namespace Engine {
namespace Net {

constexpr uint32_t kProtocolMagic = 0x41574E57u; // 'AWNW'
constexpr uint16_t kProtocolVersion = 1;
constexpr uint16_t kDefaultPort = 7777;
constexpr uint16_t kDefaultUdpPort = 7778;
constexpr uint32_t kUdpChannelMagic = 0x41574E55u; // 'AWNU'
constexpr uint32_t kInvalidUserId = 0;

using UserId = uint32_t;
using MessageTypeId = uint16_t;
using ChannelId = uint32_t;

enum class MessageType : MessageTypeId {
    Invalid = 0,

    LoginRequest = 1,
    LoginResponse = 2,
    Logout = 3,

    GenericData = 10,
    TransformState = 11,
    WorldSnapshot = 12,
    ComponentSnapshot = 13,
    EntitySnapshot = 14,

    UnreliableTransform = 40,
    UdpSessionRegister = 41,

    PeerJoined = 20,
    PeerLeft = 21,

    Ping = 30,
    Pong = 31,
};

inline const char* MessageTypeName(MessageType type) {
    switch (type) {
        case MessageType::LoginRequest: return "LoginRequest";
        case MessageType::LoginResponse: return "LoginResponse";
        case MessageType::Logout: return "Logout";
        case MessageType::GenericData: return "GenericData";
        case MessageType::TransformState: return "TransformState";
        case MessageType::WorldSnapshot: return "WorldSnapshot";
        case MessageType::ComponentSnapshot: return "ComponentSnapshot";
        case MessageType::EntitySnapshot: return "EntitySnapshot";
        case MessageType::UnreliableTransform: return "UnreliableTransform";
        case MessageType::UdpSessionRegister: return "UdpSessionRegister";
        case MessageType::PeerJoined: return "PeerJoined";
        case MessageType::PeerLeft: return "PeerLeft";
        case MessageType::Ping: return "Ping";
        case MessageType::Pong: return "Pong";
        default: return "Invalid";
    }
}

} // namespace Net
} // namespace Engine
