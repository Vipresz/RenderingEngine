#include "core/ecs/components/PlayerItemOwnershipComponent.h"

#include <algorithm>

namespace Engine {
namespace ECS {

namespace {

bool HasItemId(const std::string& itemId) {
    return !itemId.empty();
}

} // namespace

void PlayerItemOwnershipComponent::Clear() {
    ownedItems.clear();
}

uint32_t PlayerItemOwnershipComponent::CountItem(const std::string& itemId) const {
    if (!HasItemId(itemId)) {
        return 0;
    }
    uint32_t total = 0;
    for (const OwnedItemStack& stack : ownedItems) {
        if (stack.itemId == itemId && stack.IsStackable()) {
            total += stack.quantity;
        }
    }
    return total;
}

bool PlayerItemOwnershipComponent::HasItem(const std::string& itemId, uint32_t quantity) const {
    if (quantity == 0) {
        return true;
    }
    return CountItem(itemId) >= quantity;
}

bool PlayerItemOwnershipComponent::GrantItem(const std::string& itemId,
                                             uint32_t quantity,
                                             uint64_t instanceId) {
    if (!HasItemId(itemId)) {
        return false;
    }
    if (quantity == 0 || quantity > kMaxOwnedItemQuantityPerStack) {
        return false;
    }
    if (instanceId > 0) {
        if (quantity != 1) {
            return false;
        }
        for (const OwnedItemStack& stack : ownedItems) {
            if (stack.instanceId == instanceId) {
                return false;
            }
        }
        if (ownedItems.size() >= kMaxOwnedItemStacks) {
            return false;
        }
        ownedItems.push_back(OwnedItemStack{itemId, 1, instanceId});
        return true;
    }

    for (OwnedItemStack& stack : ownedItems) {
        if (stack.itemId == itemId && stack.IsStackable()) {
            const uint64_t next = static_cast<uint64_t>(stack.quantity) + static_cast<uint64_t>(quantity);
            if (next > kMaxOwnedItemQuantityPerStack) {
                return false;
            }
            stack.quantity = static_cast<uint32_t>(next);
            return true;
        }
    }

    if (ownedItems.size() >= kMaxOwnedItemStacks) {
        return false;
    }
    ownedItems.push_back(OwnedItemStack{itemId, quantity, 0});
    return true;
}

bool PlayerItemOwnershipComponent::RemoveItem(const std::string& itemId, uint32_t quantity) {
    if (!HasItemId(itemId) || quantity == 0) {
        return false;
    }
    if (!HasItem(itemId, quantity)) {
        return false;
    }

    uint32_t remaining = quantity;
    for (auto it = ownedItems.begin(); it != ownedItems.end() && remaining > 0;) {
        if (it->itemId != itemId || !it->IsStackable()) {
            ++it;
            continue;
        }
        if (it->quantity <= remaining) {
            remaining -= it->quantity;
            it = ownedItems.erase(it);
        } else {
            it->quantity -= remaining;
            remaining = 0;
        }
    }
    return remaining == 0;
}

bool PlayerItemOwnershipComponent::RemoveInstance(uint64_t instanceId) {
    if (instanceId == 0) {
        return false;
    }
    const auto it = std::find_if(ownedItems.begin(), ownedItems.end(), [&](const OwnedItemStack& stack) {
        return stack.instanceId == instanceId;
    });
    if (it == ownedItems.end()) {
        return false;
    }
    ownedItems.erase(it);
    return true;
}

bool PlayerItemOwnershipComponent::TransferItemTo(PlayerItemOwnershipComponent& recipient,
                                                  const std::string& itemId,
                                                  uint32_t quantity) {
    if (ownerUserId == recipient.ownerUserId) {
        return false;
    }
    if (!RemoveItem(itemId, quantity)) {
        return false;
    }
    if (!recipient.GrantItem(itemId, quantity)) {
        GrantItem(itemId, quantity);
        return false;
    }
    return true;
}

} // namespace ECS
} // namespace Engine
