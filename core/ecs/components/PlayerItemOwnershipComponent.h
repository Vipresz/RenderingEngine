#pragma once

#include "core/network/NetTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Engine {
namespace ECS {

constexpr uint32_t kMaxOwnedItemStacks = 256;
constexpr uint32_t kMaxOwnedItemQuantityPerStack = 1'000'000;

struct OwnedItemStack {
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
    bool GrantItem(const std::string& itemId, uint32_t quantity = 1, uint64_t instanceId = 0);
    bool RemoveItem(const std::string& itemId, uint32_t quantity = 1);
    bool RemoveInstance(uint64_t instanceId);

    bool TransferItemTo(PlayerItemOwnershipComponent& recipient,
                        const std::string& itemId,
                        uint32_t quantity = 1);
};

} // namespace ECS
} // namespace Engine
