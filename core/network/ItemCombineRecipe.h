#pragma once

#include "core/ecs/components/PlayerItemOwnershipComponent.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Engine {
namespace Net {

struct ItemCombineRecipe {
    std::string inputA;
    std::string inputB;
    std::string outputItemId;
    uint32_t outputQuantity = 1;
};

inline std::optional<ItemCombineRecipe> FindCombineRecipe(
    const std::vector<ItemCombineRecipe>& recipes,
    const std::string& itemA,
    const std::string& itemB) {
    for (const ItemCombineRecipe& recipe : recipes) {
        const bool forward = recipe.inputA == itemA && recipe.inputB == itemB;
        const bool reverse = recipe.inputA == itemB && recipe.inputB == itemA;
        if (forward || reverse) {
            return recipe;
        }
    }
    return std::nullopt;
}

using ItemStackCapResolver = std::function<uint32_t(const std::string& itemId)>;

inline bool ApplyItemCombine(ECS::PlayerItemOwnershipComponent& ownership,
                             uint16_t primarySlotIndex,
                             uint16_t secondarySlotIndex,
                             const ItemCombineRecipe& recipe,
                             ItemStackCapResolver stackCapResolver = nullptr,
                             uint16_t bagCapacity = ECS::kDefaultBagCapacity) {
    if (primarySlotIndex == secondarySlotIndex) {
        return false;
    }

    const ECS::OwnedItemStack* primaryStack = nullptr;
    const ECS::OwnedItemStack* secondaryStack = nullptr;
    for (const ECS::OwnedItemStack& stack : ownership.ownedItems) {
        if (stack.slotIndex == primarySlotIndex) {
            primaryStack = &stack;
        } else if (stack.slotIndex == secondarySlotIndex) {
            secondaryStack = &stack;
        }
    }
    if (primaryStack == nullptr || secondaryStack == nullptr || primaryStack->quantity == 0 ||
        secondaryStack->quantity == 0) {
        return false;
    }

    const bool matches =
        (primaryStack->itemId == recipe.inputA && secondaryStack->itemId == recipe.inputB) ||
        (primaryStack->itemId == recipe.inputB && secondaryStack->itemId == recipe.inputA);
    if (!matches) {
        return false;
    }

    if (!ownership.RemoveFromSlot(primarySlotIndex, 1, false) ||
        !ownership.RemoveFromSlot(secondarySlotIndex, 1, false)) {
        return false;
    }

    const uint32_t stackCap =
        stackCapResolver ? stackCapResolver(recipe.outputItemId) : 20u;
    if (!ownership.GrantItem(recipe.outputItemId, recipe.outputQuantity, 0, stackCap, bagCapacity)) {
        return false;
    }
    return true;
}

} // namespace Net
} // namespace Engine
