#include <gtest/gtest.h>

#include "core/ecs/Registry.h"

using Engine::ECS::Registry;
using Engine::EntityID;

namespace {

struct PositionComponent {
    float x = 0.0f;
    float y = 0.0f;
};

struct VelocityComponent {
    float dx = 0.0f;
    float dy = 0.0f;
};

} // namespace

TEST(ECSTest, CreateEntityAndAddComponents) {
    Registry registry;
    const auto entity = registry.CreateEntity();

    auto& position = registry.AddComponent<PositionComponent>(entity.id);
    position.x = 10.0f;
    position.y = 20.0f;

    ASSERT_TRUE(registry.HasComponent<PositionComponent>(entity.id));
    auto* fetched = registry.GetComponent<PositionComponent>(entity.id);
    ASSERT_NE(fetched, nullptr);
    EXPECT_FLOAT_EQ(fetched->x, 10.0f);
    EXPECT_FLOAT_EQ(fetched->y, 20.0f);
}

TEST(ECSTest, ViewReturnsMatchingEntities) {
    Registry registry;
    const auto a = registry.CreateEntity();
    const auto b = registry.CreateEntity();
    const auto c = registry.CreateEntity();

    registry.AddComponent<PositionComponent>(a.id);
    registry.AddComponent<PositionComponent>(b.id);
    registry.AddComponent<VelocityComponent>(b.id);
    registry.AddComponent<VelocityComponent>(c.id);

    const auto positionView = registry.View<PositionComponent>();
    EXPECT_EQ(positionView.size(), 2u);

    const auto bothView = registry.View<PositionComponent, VelocityComponent>();
    EXPECT_EQ(bothView.size(), 1u);
    EXPECT_EQ(bothView[0], b.id);
}

TEST(ECSTest, DestroyEntityRemovesComponents) {
    Registry registry;
    const auto entity = registry.CreateEntity();
    registry.AddComponent<PositionComponent>(entity.id);
    registry.DestroyEntity(entity.id);

    EXPECT_FALSE(registry.HasComponent<PositionComponent>(entity.id));
    EXPECT_EQ(registry.GetEntityCount(), 0u);
}
