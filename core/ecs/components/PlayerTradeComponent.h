#pragma once

#include "core/network/NetTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Engine {
namespace ECS {

constexpr uint32_t kMaxTradeOfferSlots = 12;

enum class TradePhase : uint8_t {
    None = 0,
    Requested = 1,
    Open = 2,
    Completed = 3,
    Cancelled = 4,
};

struct TradeOfferItem {
    std::string itemId;
    uint32_t quantity = 1;
    uint64_t instanceId = 0;
};

// Client-side view of an active trade session for UI and replication.
struct PlayerTradeComponent {
    uint64_t sessionId = 0;
    Engine::Net::UserId partnerUserId = Engine::Net::kInvalidUserId;
    TradePhase phase = TradePhase::None;
    bool selfLocked = false;
    bool partnerLocked = false;
    std::vector<TradeOfferItem> selfOffer;
    std::vector<TradeOfferItem> partnerOffer;

    bool IsActive() const { return phase == TradePhase::Requested || phase == TradePhase::Open; }
    void Clear();
};

} // namespace ECS
} // namespace Engine
