#include <algorithm>
#include <gtest/gtest.h>

#include "core/ecs/components/PlayerItemOwnershipComponent.h"
#include "core/ecs/Registry.h"
#include "network/ItemOwnershipReplicationBinding.h"
#include "network/systems/ReplicationSystem.h"

using Engine::ECS::PlayerItemOwnershipComponent;
using Engine::ECS::Registry;
using Engine::Platform::Networking::RegisterItemOwnershipReplication;
using Engine::Platform::Networking::Systems::ReplicationSystem;

TEST(ItemOwnershipReplicationBindingTest, BuildAndApplyOwnershipSnapshot) {
    Registry registry;
    ReplicationSystem replication;
    RegisterItemOwnershipReplication(replication);

    const Engine::EntityID entity = registry.CreateEntity().id;
    auto& ownership = registry.AddComponent<PlayerItemOwnershipComponent>(entity);
    ownership.ownerUserId = 9;
    ownership.GrantItem("arrow", 24);

    replication.RegisterReplicatedEntity(77, entity);
    const Engine::Net::EntitySnapshot built = replication.BuildEntitySnapshot(77, registry, 50);
    const auto ownershipSnapshot = std::find_if(
        built.components.begin(), built.components.end(), [](const Engine::Net::ComponentSnapshot& snapshot) {
            return snapshot.componentId == Engine::Net::kItemOwnershipReplicationComponent;
        });
    ASSERT_NE(ownershipSnapshot, built.components.end());

    const Engine::EntityID target = registry.CreateEntity().id;
    replication.RegisterReplicatedEntity(77, target);
    replication.ApplyEntitySnapshot(built, registry);

    const PlayerItemOwnershipComponent* applied = registry.GetComponent<PlayerItemOwnershipComponent>(target);
    ASSERT_NE(applied, nullptr);
    EXPECT_EQ(applied->ownerUserId, 9u);
    EXPECT_EQ(applied->CountItem("arrow"), 24u);
}
