#include <gtest/gtest.h>

#include "core/Types.h"
#include "core/ecs/Registry.h"
#include "network/systems/ReplicationSystem.h"
#include "render/components/TransformComponent.h"
#include "render/components/AnimationComponent.h"
#include "physics/components/ColliderComponent.h"

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

TEST(ReplicationSystemTest, BuildAndApplyAnimationSnapshotWithPlaybackFields) {
    Registry registry;
    ReplicationSystem replication;

    const EntityID entity = registry.CreateEntity().id;
    auto& anim = registry.AddComponent<Engine::Platform::Render::Components::AnimationComponent>(entity);
    anim.currentState = Engine::Platform::Render::Components::AnimationComponent::AnimationState::Running;
    anim.isJumping = true;
    anim.isRunning = true;
    anim.currentAnimationTime = 1.25f;
    anim.locomotionSpeed = 4.5f;
    anim.isLooping = false;
    anim.jumpAnimationActive = true;
    anim.landingActive = true;
    anim.airVelocityX = 1.0f;
    anim.airVelocityZ = -2.0f;
    registry.AddComponent<TransformComponent>(entity);

    replication.RegisterReplicatedEntity(7, entity);
    const Engine::Net::EntitySnapshot built = replication.BuildEntitySnapshot(7, registry, 50);
    const Engine::Net::ComponentSnapshot* animationSnapshot = nullptr;
    for (const Engine::Net::ComponentSnapshot& component : built.components) {
        if (component.componentId == Engine::Net::kAnimationReplicationComponent) {
            animationSnapshot = &component;
            break;
        }
    }
    ASSERT_NE(animationSnapshot, nullptr);

    anim.currentAnimationTime = 0.0f;
    anim.locomotionSpeed = 0.0f;
    replication.ApplyEntitySnapshot(built, registry);

    const auto* applied = registry.GetComponent<Engine::Platform::Render::Components::AnimationComponent>(entity);
    ASSERT_NE(applied, nullptr);
    EXPECT_FLOAT_EQ(applied->currentAnimationTime, 1.25f);
    EXPECT_FLOAT_EQ(applied->locomotionSpeed, 4.5f);
    EXPECT_FALSE(applied->isLooping);
    EXPECT_TRUE(applied->landingActive);
    EXPECT_FLOAT_EQ(applied->airVelocityX, 1.0f);
    EXPECT_FLOAT_EQ(applied->airVelocityZ, -2.0f);
}

TEST(ReplicationSystemTest, BuildAndApplyColliderSnapshot) {
    Registry registry;
    ReplicationSystem replication;

    const EntityID entity = registry.CreateEntity().id;
    auto& collider = registry.AddComponent<Engine::Physics::Components::ColliderComponent>(entity);
    collider.localBounds.min = glm::vec3(-1.0f, 0.0f, -1.0f);
    collider.localBounds.max = glm::vec3(1.0f, 2.0f, 1.0f);
    collider.isStatic = true;
    collider.useGravity = false;
    collider.velocity = glm::vec3(0.5f, 0.0f, -0.25f);
    registry.AddComponent<TransformComponent>(entity);

    replication.RegisterReplicatedEntity(9, entity);
    const Engine::Net::EntitySnapshot built = replication.BuildEntitySnapshot(9, registry, 1);

    collider.velocity = glm::vec3(0.0f);
    collider.isStatic = false;
    replication.ApplyEntitySnapshot(built, registry);

    const auto* applied = registry.GetComponent<Engine::Physics::Components::ColliderComponent>(entity);
    ASSERT_NE(applied, nullptr);
    EXPECT_TRUE(applied->isStatic);
    EXPECT_FALSE(applied->useGravity);
    EXPECT_FLOAT_EQ(applied->velocity.x, 0.5f);
    EXPECT_FLOAT_EQ(applied->localBounds.max.y, 2.0f);
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
