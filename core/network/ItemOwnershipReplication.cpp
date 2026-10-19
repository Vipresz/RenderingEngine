#include "core/network/ItemOwnershipReplication.h"

namespace Engine {
namespace Net {

ECS::PlayerItemOwnershipComponent ToOwnershipComponent(const ReplicatedItemOwnership& replicated) {
    ECS::PlayerItemOwnershipComponent component;
    component.ownerUserId = replicated.ownerUserId;
    component.ownedItems.reserve(replicated.items.size());
    for (const ReplicatedOwnedItem& item : replicated.items) {
        ECS::OwnedItemStack stack;
        stack.slotIndex = item.slotIndex;
        stack.itemId = item.itemId;
        stack.quantity = item.quantity;
        stack.instanceId = item.instanceId;
        component.ownedItems.push_back(std::move(stack));
    }
    return component;
}

ReplicatedItemOwnership FromOwnershipComponent(const ECS::PlayerItemOwnershipComponent& component) {
    ReplicatedItemOwnership replicated;
    replicated.ownerUserId = component.ownerUserId;
    replicated.items.reserve(component.ownedItems.size());
    for (const ECS::OwnedItemStack& stack : component.ownedItems) {
        ReplicatedOwnedItem item;
        item.slotIndex = stack.slotIndex;
        item.itemId = stack.itemId;
        item.quantity = stack.quantity;
        item.instanceId = stack.instanceId;
        replicated.items.push_back(std::move(item));
    }
    return replicated;
}

} // namespace Net
} // namespace Engine
