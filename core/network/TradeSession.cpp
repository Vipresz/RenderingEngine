#include "core/network/TradeSession.h"

#include <algorithm>
#include <unordered_set>

namespace Engine {
namespace Net {

namespace {

bool HasItemId(const std::string& itemId) {
    return !itemId.empty();
}

bool RemoveOfferItem(ECS::PlayerItemOwnershipComponent& ownership, const TradeOfferItem& offer) {
    if (offer.instanceId > 0) {
        return ownership.RemoveInstance(offer.instanceId);
    }
    return ownership.RemoveItem(offer.itemId, offer.quantity);
}

} // namespace

bool TradeSession::Involves(UserId userId) const {
    return userId == initiatorUserId || userId == partnerUserId;
}

std::vector<TradeOfferItem>* TradeSession::OfferFor(UserId userId) {
    if (userId == initiatorUserId) {
        return &initiatorOffer;
    }
    if (userId == partnerUserId) {
        return &partnerOffer;
    }
    return nullptr;
}

const std::vector<TradeOfferItem>* TradeSession::OfferFor(UserId userId) const {
    if (userId == initiatorUserId) {
        return &initiatorOffer;
    }
    if (userId == partnerUserId) {
        return &partnerOffer;
    }
    return nullptr;
}

bool* TradeSession::LockFor(UserId userId) {
    if (userId == initiatorUserId) {
        return &initiatorLocked;
    }
    if (userId == partnerUserId) {
        return &partnerLocked;
    }
    return nullptr;
}

const bool* TradeSession::LockFor(UserId userId) const {
    if (userId == initiatorUserId) {
        return &initiatorLocked;
    }
    if (userId == partnerUserId) {
        return &partnerLocked;
    }
    return nullptr;
}

UserId TradeSession::PartnerOf(UserId userId) const {
    if (userId == initiatorUserId) {
        return partnerUserId;
    }
    if (userId == partnerUserId) {
        return initiatorUserId;
    }
    return kInvalidUserId;
}

bool IsValidTradeOffer(const std::vector<TradeOfferItem>& offer) {
    if (offer.size() > kMaxTradeOfferSlots) {
        return false;
    }

    std::unordered_set<uint64_t> uniqueInstances;
    for (const TradeOfferItem& item : offer) {
        if (!HasItemId(item.itemId) || item.quantity == 0) {
            return false;
        }
        if (item.instanceId > 0) {
            if (item.quantity != 1 || !uniqueInstances.insert(item.instanceId).second) {
                return false;
            }
        }
    }
    return true;
}

bool CanAffordOffer(const ECS::PlayerItemOwnershipComponent& ownership,
                    const std::vector<TradeOfferItem>& offer) {
    if (!IsValidTradeOffer(offer)) {
        return false;
    }

    ECS::PlayerItemOwnershipComponent scratch = ownership;
    for (const TradeOfferItem& item : offer) {
        if (item.instanceId > 0) {
            if (!scratch.RemoveInstance(item.instanceId)) {
                return false;
            }
        } else if (!scratch.RemoveItem(item.itemId, item.quantity)) {
            return false;
        }
    }
    return true;
}

bool ExecuteTradeExchange(ECS::PlayerItemOwnershipComponent& leftOwnership,
                          ECS::PlayerItemOwnershipComponent& rightOwnership,
                          const std::vector<TradeOfferItem>& leftOffer,
                          const std::vector<TradeOfferItem>& rightOffer) {
    if (!IsValidTradeOffer(leftOffer) || !IsValidTradeOffer(rightOffer)) {
        return false;
    }
    if (!CanAffordOffer(leftOwnership, leftOffer) || !CanAffordOffer(rightOwnership, rightOffer)) {
        return false;
    }

    ECS::PlayerItemOwnershipComponent leftSnapshot = leftOwnership;
    ECS::PlayerItemOwnershipComponent rightSnapshot = rightOwnership;

    for (const TradeOfferItem& item : leftOffer) {
        if (!RemoveOfferItem(leftOwnership, item)) {
            leftOwnership = leftSnapshot;
            rightOwnership = rightSnapshot;
            return false;
        }
    }
    for (const TradeOfferItem& item : rightOffer) {
        if (!RemoveOfferItem(rightOwnership, item)) {
            leftOwnership = leftSnapshot;
            rightOwnership = rightSnapshot;
            return false;
        }
    }

    for (const TradeOfferItem& item : rightOffer) {
        if (!leftOwnership.GrantItem(item.itemId, item.quantity, item.instanceId)) {
            leftOwnership = leftSnapshot;
            rightOwnership = rightSnapshot;
            return false;
        }
    }
    for (const TradeOfferItem& item : leftOffer) {
        if (!rightOwnership.GrantItem(item.itemId, item.quantity, item.instanceId)) {
            leftOwnership = leftSnapshot;
            rightOwnership = rightSnapshot;
            return false;
        }
    }

    return true;
}

ECS::PlayerTradeComponent BuildTradeView(const TradeSession& session, UserId viewerUserId) {
    ECS::PlayerTradeComponent view;
    if (!session.Involves(viewerUserId)) {
        return view;
    }

    view.sessionId = session.sessionId;
    view.partnerUserId = session.PartnerOf(viewerUserId);
    view.phase = session.phase;

    const bool viewerIsInitiator = viewerUserId == session.initiatorUserId;
    view.selfLocked = viewerIsInitiator ? session.initiatorLocked : session.partnerLocked;
    view.partnerLocked = viewerIsInitiator ? session.partnerLocked : session.initiatorLocked;

    if (viewerIsInitiator) {
        view.selfOffer = session.initiatorOffer;
        view.partnerOffer = session.partnerOffer;
    } else {
        view.selfOffer = session.partnerOffer;
        view.partnerOffer = session.initiatorOffer;
    }
    return view;
}

NetBuffer SerializeTradeCommand(const TradeCommand& command) {
    NetBuffer buffer;
    buffer.WriteU8(static_cast<uint8_t>(command.action));
    buffer.WriteU32(command.targetUserId);
    const uint16_t count = static_cast<uint16_t>(command.offer.size());
    buffer.WriteU16(count);
    for (const TradeOfferItem& item : command.offer) {
        buffer.WriteString(item.itemId);
        buffer.WriteU32(item.quantity);
        buffer.WriteU64(item.instanceId);
    }
    return buffer;
}

bool DeserializeTradeCommand(const NetBuffer& buffer, TradeCommand& out) {
    NetBuffer copy = buffer;
    copy.ResetRead();
    uint8_t action = 0;
    uint16_t count = 0;
    if (!copy.ReadU8(action) || !copy.ReadU32(out.targetUserId) || !copy.ReadU16(count)) {
        return false;
    }
    out.action = static_cast<TradeAction>(action);
    out.offer.clear();
    out.offer.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        TradeOfferItem item;
        if (!copy.ReadString(item.itemId) || !copy.ReadU32(item.quantity) ||
            !copy.ReadU64(item.instanceId)) {
            return false;
        }
        out.offer.push_back(std::move(item));
    }
    return true;
}

} // namespace Net
} // namespace Engine
