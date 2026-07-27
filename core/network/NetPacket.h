#pragma once

#include "core/network/NetBuffer.h"
#include "core/network/NetTypes.h"

#include <cstdint>
#include <vector>

namespace Engine {
namespace Net {

struct MessageHeader {
    uint32_t magic = kProtocolMagic;
    uint16_t version = kProtocolVersion;
    MessageTypeId type = static_cast<MessageTypeId>(MessageType::Invalid);
    uint32_t payloadSize = 0;
    uint64_t sequence = 0;
};

struct NetPacket {
    MessageHeader header;
    NetBuffer payload;

    MessageType GetType() const { return static_cast<MessageType>(header.type); }
};

inline constexpr size_t kMessageHeaderSize = sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t) +
                                            sizeof(uint32_t) + sizeof(uint64_t);

inline NetBuffer SerializePacket(const NetPacket& message) {
    NetBuffer buffer;
    buffer.WriteU32(message.header.magic);
    buffer.WriteU16(message.header.version);
    buffer.WriteU16(message.header.type);
    buffer.WriteU32(static_cast<uint32_t>(message.payload.Size()));
    buffer.WriteU64(message.header.sequence);
    if (!message.payload.Data().empty()) {
        buffer.WriteBytes(message.payload.Data().data(), message.payload.Data().size());
    }
    return buffer;
}

inline bool DeserializePacket(const uint8_t* data, size_t size, NetPacket& outMessage) {
    if (!data || size < kMessageHeaderSize) {
        return false;
    }

    NetBuffer buffer(std::vector<uint8_t>(data, data + size));
    if (!buffer.ReadU32(outMessage.header.magic) ||
        !buffer.ReadU16(outMessage.header.version) ||
        !buffer.ReadU16(outMessage.header.type) ||
        !buffer.ReadU32(outMessage.header.payloadSize) ||
        !buffer.ReadU64(outMessage.header.sequence)) {
        return false;
    }

    if (outMessage.header.magic != kProtocolMagic ||
        outMessage.header.version != kProtocolVersion) {
        return false;
    }

    if (outMessage.header.payloadSize > kMaxPayloadBytes) {
        return false;
    }

    if (buffer.Remaining() != outMessage.header.payloadSize) {
        return false;
    }

    outMessage.payload.Clear();
    if (outMessage.header.payloadSize > 0) {
        std::vector<uint8_t> payload(outMessage.header.payloadSize);
        if (!buffer.ReadBytes(payload.data(), payload.size())) {
            return false;
        }
        outMessage.payload = NetBuffer(std::move(payload));
    }
    return true;
}

} // namespace Net
} // namespace Engine
