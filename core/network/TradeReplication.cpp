#include "core/network/TradeReplication.h"

namespace Engine {
namespace Net {

namespace {

ReplicatedTradeOfferItem ToReplicatedOffer(const ECS::TradeOfferItem& item) {
    ReplicatedTradeOfferItem replicated;
    replicated.itemId = item.itemId;
    replicated.quantity = item.quantity;
    replicated.instanceId = item.instanceId;
    return replicated;
}

ECS::TradeOfferItem FromReplicatedOffer(const ReplicatedTradeOfferItem& item) {
    ECS::TradeOfferItem offer;
    offer.itemId = item.itemId;
    offer.quantity = item.quantity;
    offer.instanceId = item.instanceId;
    return offer;
}

} // namespace

ECS::PlayerTradeComponent ToTradeComponent(const ReplicatedPlayerTrade& replicated) {
    ECS::PlayerTradeComponent component;
    component.sessionId = replicated.sessionId;
    component.partnerUserId = replicated.partnerUserId;
    component.phase = static_cast<ECS::TradePhase>(replicated.phase);
    component.selfLocked = replicated.selfLocked;
    component.partnerLocked = replicated.partnerLocked;
    component.selfOffer.reserve(replicated.selfOffer.size());
    for (const ReplicatedTradeOfferItem& item : replicated.selfOffer) {
        component.selfOffer.push_back(FromReplicatedOffer(item));
    }
    component.partnerOffer.reserve(replicated.partnerOffer.size());
    for (const ReplicatedTradeOfferItem& item : replicated.partnerOffer) {
        component.partnerOffer.push_back(FromReplicatedOffer(item));
    }
    return component;
}

ReplicatedPlayerTrade FromTradeComponent(const ECS::PlayerTradeComponent& component) {
    return FromTradeView(component);
}

ReplicatedPlayerTrade FromTradeView(const ECS::PlayerTradeComponent& view) {
    ReplicatedPlayerTrade replicated;
    replicated.sessionId = view.sessionId;
    replicated.partnerUserId = view.partnerUserId;
    replicated.phase = static_cast<uint8_t>(view.phase);
    replicated.selfLocked = view.selfLocked;
    replicated.partnerLocked = view.partnerLocked;
    for (const ECS::TradeOfferItem& item : view.selfOffer) {
        replicated.selfOffer.push_back(ToReplicatedOffer(item));
    }
    for (const ECS::TradeOfferItem& item : view.partnerOffer) {
        replicated.partnerOffer.push_back(ToReplicatedOffer(item));
    }
    return replicated;
}

} // namespace Net
} // namespace Engine
