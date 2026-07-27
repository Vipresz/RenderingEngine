#include <gtest/gtest.h>

#include "core/Types.h"
#include "core/ecs/Registry.h"
#include "network/systems/ReplicationSystem.h"
#include "render/components/TransformComponent.h"
#include "render/components/AnimationComponent.h"

#include <glm/glm.hpp>

using Engine::EntityID;
using Engine::ECS::Registry;
using Engine::Platform::Networking::Systems::ReplicationSystem;
using Engine::Platform::Render::Components::TransformComponent;

TEST(ReplicationSystemTest, BuildAndApplyTransformSnapshot) {
    Registry registry;
    ReplicationSystem replication;

    const EntityID entity = registry.CreateEntity().id;
    auto& transform = registry.AddComponent<TransformComponent>(entity);
    transform.position = glm::vec3(4.0f, 5.0f, 6.0f);
    transform.rotation = glm::vec3(10.0f, 20.0f, 30.0f);
    transform.scale = glm::vec3(2.0f, 2.0f, 2.0f);
    registry.AddComponent<Engine::Platform::Render::Components::AnimationComponent>(entity);

    replication.RegisterReplicatedEntity(42, entity);
    Engine::Net::EntitySnapshot built = replication.BuildEntitySnapshot(42, registry, 100);
    ASSERT_GE(built.components.size(), 1u);
    EXPECT_EQ(built.components[0].componentId, Engine::Net::kTransformReplicationComponent);

    transform.position = glm::vec3(0.0f);
    const EntityID target = registry.CreateEntity().id;
    auto& targetTransform = registry.AddComponent<TransformComponent>(target);
    targetTransform.scale = glm::vec3(0.08f, 0.08f, 0.08f);
    replication.RegisterReplicatedEntity(42, target);
    replication.ApplyEntitySnapshot(built, registry);

    const TransformComponent* applied = registry.GetComponent<TransformComponent>(target);
    ASSERT_NE(applied, nullptr);
    EXPECT_FLOAT_EQ(applied->position.x, 4.0f);
    EXPECT_FLOAT_EQ(applied->rotation.y, 20.0f);
    // Networked transform must not overwrite presentation scale.
    EXPECT_FLOAT_EQ(applied->scale.z, 0.08f);
}

TEST(ReplicationSystemTest, PeriodicSendInvokesHandler) {
    Registry registry;
    ReplicationSystem replication;

    const EntityID entity = registry.CreateEntity().id;
    registry.AddComponent<TransformComponent>(entity);
    replication.RegisterReplicatedEntity(1, entity);

    bool sent = false;
    replication.SetSnapshotSendHandler([&](const Engine::Net::EntitySnapshot& snapshot) {
        sent = true;
        EXPECT_EQ(snapshot.entityId, 1u);
        EXPECT_FALSE(snapshot.components.empty());
    });

    replication.Update(0.2f, registry, 1);
    EXPECT_TRUE(sent);
}
