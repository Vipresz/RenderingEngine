#pragma once

#include "core/network/NetTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Engine {
namespace ECS {

constexpr uint32_t kMaxOwnedItemStacks = 256;
constexpr uint32_t kMaxOwnedItemQuantityPerStack = 1'000'000;
constexpr uint16_t kInvalidBagSlotIndex = static_cast<uint16_t>(-1);
constexpr uint16_t kDefaultBagCapacity = 24;

struct OwnedItemStack {
    uint16_t slotIndex = 0;
    std::string itemId;
    uint32_t quantity = 1;
    uint64_t instanceId = 0; // 0 = fungible stack keyed by itemId

    bool IsStackable() const { return instanceId == 0; }
};

// Tracks items owned by a player entity (bag/inventory authority, distinct from equipped loadout).
struct PlayerItemOwnershipComponent {
    Engine::Net::UserId ownerUserId = Engine::Net::kInvalidUserId;
    std::vector<OwnedItemStack> ownedItems;

    void Clear();

    uint32_t CountItem(const std::string& itemId) const;
    bool HasItem(const std::string& itemId, uint32_t quantity = 1) const;

    // Grants stackable items by itemId. Unique items require instanceId > 0.
    bool GrantItem(const std::string& itemId,
                   uint32_t quantity = 1,
                   uint64_t instanceId = 0,
                   uint32_t maxStackSize = 20,
                   uint16_t bagCapacity = kDefaultBagCapacity);
    bool RemoveItem(const std::string& itemId, uint32_t quantity = 1);
    bool RemoveFromSlot(uint16_t slotIndex, uint32_t quantity = 1, bool removeEntireStack = false);
    bool RemoveInstance(uint64_t instanceId);

    bool TransferItemTo(PlayerItemOwnershipComponent& recipient,
                        const std::string& itemId,
                        uint32_t quantity = 1,
                        uint32_t maxStackSize = 20,
                        uint16_t bagCapacity = kDefaultBagCapacity);
};

} // namespace ECS
} // namespace Engine
