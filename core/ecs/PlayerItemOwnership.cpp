#include "core/ecs/components/PlayerItemOwnershipComponent.h"

#include <algorithm>

namespace Engine {
namespace ECS {

namespace {

bool HasItemId(const std::string& itemId) {
    return !itemId.empty();
}

bool IsSlotOccupied(const std::vector<OwnedItemStack>& items, uint16_t slotIndex) {
    for (const OwnedItemStack& stack : items) {
        if (stack.slotIndex == slotIndex) {
            return true;
        }
    }
    return false;
}

uint16_t FindFreeBagSlot(const std::vector<OwnedItemStack>& items, uint16_t bagCapacity) {
    for (uint16_t slot = 0; slot < bagCapacity; ++slot) {
        if (!IsSlotOccupied(items, slot)) {
            return slot;
        }
    }
    return kInvalidBagSlotIndex;
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
                                             uint64_t instanceId,
                                             uint32_t maxStackSize,
                                             uint16_t bagCapacity) {
    if (!HasItemId(itemId)) {
        return false;
    }
    if (quantity == 0 || quantity > kMaxOwnedItemQuantityPerStack) {
        return false;
    }
    if (maxStackSize == 0) {
        return false;
    }
    if (bagCapacity == 0) {
        return false;
    }

    const std::vector<OwnedItemStack> snapshot = ownedItems;

    if (instanceId > 0) {
        if (quantity != 1 || maxStackSize < 1) {
            return false;
        }
        for (const OwnedItemStack& stack : ownedItems) {
            if (stack.instanceId == instanceId) {
                return false;
            }
        }
        const uint16_t slot = FindFreeBagSlot(ownedItems, bagCapacity);
        if (slot == kInvalidBagSlotIndex) {
            return false;
        }
        ownedItems.push_back(OwnedItemStack{slot, itemId, 1, instanceId});
        return true;
    }

    uint32_t remaining = quantity;
    for (OwnedItemStack& stack : ownedItems) {
        if (stack.itemId != itemId || !stack.IsStackable() || stack.quantity >= maxStackSize) {
            continue;
        }
        const uint32_t space = maxStackSize - stack.quantity;
        const uint32_t toAdd = std::min(space, remaining);
        stack.quantity += toAdd;
        remaining -= toAdd;
        if (remaining == 0) {
            return true;
        }
    }

    while (remaining > 0) {
        const uint16_t slot = FindFreeBagSlot(ownedItems, bagCapacity);
        if (slot == kInvalidBagSlotIndex || ownedItems.size() >= kMaxOwnedItemStacks) {
            ownedItems = snapshot;
            return false;
        }
        const uint32_t toAdd = std::min(remaining, maxStackSize);
        ownedItems.push_back(OwnedItemStack{slot, itemId, toAdd, 0});
        remaining -= toAdd;
    }

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

bool PlayerItemOwnershipComponent::RemoveFromSlot(uint16_t slotIndex,
                                                  uint32_t quantity,
                                                  bool removeEntireStack) {
    if (removeEntireStack) {
        quantity = 0;
    } else if (quantity == 0) {
        return false;
    }

    const auto it = std::find_if(
        ownedItems.begin(), ownedItems.end(), [&](const OwnedItemStack& stack) {
            return stack.slotIndex == slotIndex;
        });
    if (it == ownedItems.end()) {
        return false;
    }

    if (removeEntireStack || quantity >= it->quantity) {
        ownedItems.erase(it);
        return true;
    }

    it->quantity -= quantity;
    return true;
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
                                                  uint32_t quantity,
                                                  uint32_t maxStackSize,
                                                  uint16_t bagCapacity) {
    if (ownerUserId == recipient.ownerUserId) {
        return false;
    }

    const std::vector<OwnedItemStack> giverSnapshot = ownedItems;
    const std::vector<OwnedItemStack> recipientSnapshot = recipient.ownedItems;

    if (!RemoveItem(itemId, quantity)) {
        return false;
    }
    if (!recipient.GrantItem(itemId, quantity, 0, maxStackSize, bagCapacity)) {
        ownedItems = giverSnapshot;
        recipient.ownedItems = recipientSnapshot;
        return false;
    }
    return true;
}

} // namespace ECS
} // namespace Engine
