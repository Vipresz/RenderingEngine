#pragma once

#include "core/ecs/components/PlayerTradeComponent.h"
#include "core/network/NetBuffer.h"
#include "core/network/NetTypes.h"
#include "core/network/TradeSession.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Engine {
namespace Net {

struct ReplicatedTradeOfferItem {
    std::string itemId;
    uint32_t quantity = 1;
    uint64_t instanceId = 0;
    uint16_t bagSlotIndex = ECS::kInvalidBagSlotIndex;
};

struct ReplicatedPlayerTrade {
    uint64_t sessionId = 0;
    UserId partnerUserId = kInvalidUserId;
    UserId requesterUserId = kInvalidUserId;
    uint8_t phase = static_cast<uint8_t>(ECS::TradePhase::None);
    bool selfLocked = false;
    bool partnerLocked = false;
    std::vector<ReplicatedTradeOfferItem> selfOffer;
    std::vector<ReplicatedTradeOfferItem> partnerOffer;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU64(sessionId);
        buffer.WriteU32(partnerUserId);
        buffer.WriteU8(phase);
        buffer.WriteU8(selfLocked ? 1 : 0);
        buffer.WriteU8(partnerLocked ? 1 : 0);

        const uint16_t selfCount = static_cast<uint16_t>(selfOffer.size());
        buffer.WriteU16(selfCount);
        for (const ReplicatedTradeOfferItem& item : selfOffer) {
            buffer.WriteString(item.itemId);
            buffer.WriteU32(item.quantity);
            buffer.WriteU64(item.instanceId);
            buffer.WriteU16(item.bagSlotIndex);
        }

        const uint16_t partnerCount = static_cast<uint16_t>(partnerOffer.size());
        buffer.WriteU16(partnerCount);
        for (const ReplicatedTradeOfferItem& item : partnerOffer) {
            buffer.WriteString(item.itemId);
            buffer.WriteU32(item.quantity);
            buffer.WriteU64(item.instanceId);
            buffer.WriteU16(item.bagSlotIndex);
        }
        buffer.WriteU32(requesterUserId);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, ReplicatedPlayerTrade& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        uint8_t selfLocked = 0;
        uint8_t partnerLocked = 0;
        uint16_t selfCount = 0;
        uint16_t partnerCount = 0;
        if (!copy.ReadU64(out.sessionId) || !copy.ReadU32(out.partnerUserId) || !copy.ReadU8(out.phase) ||
            !copy.ReadU8(selfLocked) || !copy.ReadU8(partnerLocked) || !copy.ReadU16(selfCount)) {
            return false;
        }
        out.selfLocked = selfLocked != 0;
        out.partnerLocked = partnerLocked != 0;
        out.requesterUserId = kInvalidUserId;
        if (selfCount > kMaxTradeOfferSlots) {
            return false;
        }
        out.selfOffer.clear();
        out.selfOffer.reserve(selfCount);
        for (uint16_t i = 0; i < selfCount; ++i) {
            ReplicatedTradeOfferItem item;
            if (!copy.ReadString(item.itemId) || !copy.ReadU32(item.quantity) ||
                !copy.ReadU64(item.instanceId)) {
                return false;
            }
            if (copy.Remaining() >= sizeof(uint16_t)) {
                if (!copy.ReadU16(item.bagSlotIndex)) {
                    return false;
                }
            } else {
                item.bagSlotIndex = ECS::kInvalidBagSlotIndex;
            }
            out.selfOffer.push_back(std::move(item));
        }
        if (!copy.ReadU16(partnerCount)) {
            return false;
        }
        if (partnerCount > kMaxTradeOfferSlots) {
            return false;
        }
        out.partnerOffer.clear();
        out.partnerOffer.reserve(partnerCount);
        for (uint16_t i = 0; i < partnerCount; ++i) {
            ReplicatedTradeOfferItem item;
            if (!copy.ReadString(item.itemId) || !copy.ReadU32(item.quantity) ||
                !copy.ReadU64(item.instanceId)) {
                return false;
            }
            if (copy.Remaining() >= sizeof(uint16_t)) {
                if (!copy.ReadU16(item.bagSlotIndex)) {
                    return false;
                }
            } else {
                item.bagSlotIndex = ECS::kInvalidBagSlotIndex;
            }
            out.partnerOffer.push_back(std::move(item));
        }
        if (copy.Remaining() >= sizeof(uint32_t)) {
            copy.ReadU32(out.requesterUserId);
        }
        return true;
    }
};

ECS::PlayerTradeComponent ToTradeComponent(const ReplicatedPlayerTrade& replicated);
ReplicatedPlayerTrade FromTradeComponent(const ECS::PlayerTradeComponent& component);
ReplicatedPlayerTrade FromTradeView(const ECS::PlayerTradeComponent& view);

} // namespace Net
} // namespace Engine
