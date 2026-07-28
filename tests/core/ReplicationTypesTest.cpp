#include <gtest/gtest.h>

#include "core/network/ReplicationTypes.h"

using Engine::Net::ComponentSnapshot;
using Engine::Net::EntitySnapshot;
using Engine::Net::WorldSnapshotPayload;

TEST(ReplicationTypesTest, ComponentSnapshotRoundTrip) {
    ComponentSnapshot original;
    original.entityId = 10;
    original.componentId = 3;
    original.tick = 500;
    original.data = {9, 8, 7};

    ComponentSnapshot decoded;
    ASSERT_TRUE(ComponentSnapshot::Deserialize(original.Serialize(), decoded));
    EXPECT_EQ(decoded.entityId, 10u);
    EXPECT_EQ(decoded.componentId, 3);
    EXPECT_EQ(decoded.tick, 500u);
    EXPECT_EQ(decoded.data, original.data);
}

TEST(ReplicationTypesTest, EntitySnapshotRoundTrip) {
    EntitySnapshot original;
    original.entityId = 5;
    original.tick = 100;

    ComponentSnapshot component;
    component.entityId = 5;
    component.componentId = 1;
    component.tick = 100;
    component.data = {1, 2};
    original.components.push_back(component);

    EntitySnapshot decoded;
    ASSERT_TRUE(EntitySnapshot::Deserialize(original.Serialize(), decoded));
    EXPECT_EQ(decoded.entityId, 5u);
    EXPECT_EQ(decoded.tick, 100u);
    ASSERT_EQ(decoded.components.size(), 1u);
    EXPECT_EQ(decoded.components[0].data, component.data);
}

TEST(ReplicationTypesTest, WorldSnapshotRoundTrip) {
    WorldSnapshotPayload original;
    original.tick = 999;

    EntitySnapshot entity;
    entity.entityId = 1;
    entity.tick = 999;
    original.entities.push_back(entity);

    WorldSnapshotPayload decoded;
    ASSERT_TRUE(WorldSnapshotPayload::Deserialize(original.Serialize(), decoded));
    EXPECT_EQ(decoded.tick, 999u);
    ASSERT_EQ(decoded.entities.size(), 1u);
    EXPECT_EQ(decoded.entities[0].entityId, 1u);
}
