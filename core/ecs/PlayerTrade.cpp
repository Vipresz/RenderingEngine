#include "core/ecs/components/PlayerTradeComponent.h"

namespace Engine {
namespace ECS {

void PlayerTradeComponent::Clear() {
    sessionId = 0;
    partnerUserId = Engine::Net::kInvalidUserId;
    requesterUserId = Engine::Net::kInvalidUserId;
    phase = TradePhase::None;
    selfLocked = false;
    partnerLocked = false;
    selfOffer.clear();
    partnerOffer.clear();
}

} // namespace ECS
} // namespace Engine
