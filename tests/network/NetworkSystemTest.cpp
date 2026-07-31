#include <gtest/gtest.h>

#include "network/systems/NetworkSystem.h"

#include "core/network/ReplicationTypes.h"
#include "core/ecs/Registry.h"

using Engine::ECS::Registry;
using Engine::Platform::Networking::Systems::NetworkSystem;

TEST(NetworkSystemTest, HasExpectedPriority) {
    NetworkSystem system;
    EXPECT_EQ(system.GetPriority(), 55);
}

TEST(NetworkSystemTest, StartsDisconnected) {
    NetworkSystem system;
    EXPECT_FALSE(system.IsLoggedIn());
}

TEST(NetworkSystemTest, DefersEntitySnapshotsUntilProcessNetworkFrame) {
    NetworkSystem system;
    Registry registry;

    size_t appliedCount = 0;
    system.SetEntitySnapshotHandler([&](const Engine::Net::EntitySnapshot& /*snapshot*/) {
        ++appliedCount;
    });

    Engine::Net::EntitySnapshot snapshot;
    snapshot.entityId = 42;
    system.StageEntitySnapshot(snapshot);

    EXPECT_EQ(appliedCount, 0u);
    EXPECT_EQ(system.PendingEntitySnapshotCount(), 1u);

    system.ProcessNetworkFrame(1.0f / 60.0f, registry, 1);
    EXPECT_EQ(appliedCount, 1u);
    EXPECT_EQ(system.PendingEntitySnapshotCount(), 0u);
}

TEST(NetworkSystemTest, DefersComponentSnapshotsUntilProcessNetworkFrame) {
    NetworkSystem system;
    Registry localRegistry;

    size_t appliedCount = 0;
    system.SetComponentSnapshotHandler([&](const Engine::Net::ComponentSnapshot& /*snapshot*/) {
        ++appliedCount;
    });

    Engine::Net::ComponentSnapshot snapshot;
    snapshot.entityId = 7;
    snapshot.componentId = Engine::Net::kTransformReplicationComponent;
    system.StageComponentSnapshot(snapshot);

    EXPECT_EQ(appliedCount, 0u);
    EXPECT_EQ(system.PendingComponentSnapshotCount(), 1u);

    system.ProcessNetworkFrame(1.0f / 60.0f, localRegistry, 1);
    EXPECT_EQ(appliedCount, 1u);
    EXPECT_EQ(system.PendingComponentSnapshotCount(), 0u);
}

TEST(NetworkSystemTest, GreedySendCanBeDisabled) {
    NetworkSystem system;
    system.SetGreedySend(false);
    system.SetSendRateHz(20.0f);
    EXPECT_FALSE(system.IsLoggedIn());
}
