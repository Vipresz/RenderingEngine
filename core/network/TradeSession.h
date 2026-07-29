#pragma once

#include "core/ecs/components/PlayerItemOwnershipComponent.h"
#include "core/ecs/components/PlayerTradeComponent.h"
#include "core/network/NetBuffer.h"
#include "core/network/NetTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Engine {
namespace Net {

using TradeOfferItem = ECS::TradeOfferItem;

constexpr uint32_t kMaxTradeOfferSlots = ECS::kMaxTradeOfferSlots;

enum class TradeAction : uint8_t {
    Request = 0,
    Accept = 1,
    Decline = 2,
    Cancel = 3,
    SetOffer = 4,
    Lock = 5,
    Unlock = 6,
};

struct TradeCommand {
    TradeAction action = TradeAction::Cancel;
    UserId targetUserId = kInvalidUserId;
    std::vector<TradeOfferItem> offer;
};

struct TradeSession {
    uint64_t sessionId = 0;
    UserId initiatorUserId = kInvalidUserId;
    UserId partnerUserId = kInvalidUserId;
    ECS::TradePhase phase = ECS::TradePhase::None;
    std::vector<TradeOfferItem> initiatorOffer;
    std::vector<TradeOfferItem> partnerOffer;
    bool initiatorLocked = false;
    bool partnerLocked = false;

    bool Involves(UserId userId) const;
    bool IsOpen() const { return phase == ECS::TradePhase::Open; }
    bool BothLocked() const { return initiatorLocked && partnerLocked; }

    std::vector<TradeOfferItem>* OfferFor(UserId userId);
    const std::vector<TradeOfferItem>* OfferFor(UserId userId) const;
    bool* LockFor(UserId userId);
    const bool* LockFor(UserId userId) const;
    UserId PartnerOf(UserId userId) const;
};

bool IsValidTradeOffer(const std::vector<TradeOfferItem>& offer);
bool CanAffordOffer(const ECS::PlayerItemOwnershipComponent& ownership,
                    const std::vector<TradeOfferItem>& offer);
bool ExecuteTradeExchange(ECS::PlayerItemOwnershipComponent& leftOwnership,
                          ECS::PlayerItemOwnershipComponent& rightOwnership,
                          const std::vector<TradeOfferItem>& leftOffer,
                          const std::vector<TradeOfferItem>& rightOffer);

ECS::PlayerTradeComponent BuildTradeView(const TradeSession& session, UserId viewerUserId);

NetBuffer SerializeTradeCommand(const TradeCommand& command);
bool DeserializeTradeCommand(const NetBuffer& buffer, TradeCommand& out);

} // namespace Net
} // namespace Engine
