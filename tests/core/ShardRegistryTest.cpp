#include <gtest/gtest.h>

#include "core/network/ShardRegistry.h"

TEST(ShardRegistryTest, SelectsLeastLoadedShard) {
    Engine::Net::ShardRegistry registry;
    Engine::Net::ShardEndpoint shardA;
    shardA.shardId = 1;
    shardA.host = "127.0.0.1";
    shardA.tcpPort = 7001;
    shardA.maxPlayers = 256;
    Engine::Net::ShardEndpoint shardB = shardA;
    shardB.shardId = 2;
    shardB.tcpPort = 7002;
    registry.RegisterShard(shardA);
    registry.RegisterShard(shardB);

    const uint64_t nowMs = 100000;
    registry.UpdatePopulation(1, 200, nowMs);
    registry.UpdatePopulation(2, 50, nowMs);

    const auto selected = registry.SelectShardForLogin(nowMs);
    ASSERT_TRUE(selected.has_value());
    EXPECT_EQ(selected->shardId, 2u);
}

TEST(ShardRegistryTest, IgnoresStalePopulation) {
    Engine::Net::ShardRegistry registry;
    Engine::Net::ShardEndpoint shard;
    shard.shardId = 1;
    shard.host = "127.0.0.1";
    shard.tcpPort = 7001;
    shard.maxPlayers = 256;
    registry.RegisterShard(shard);
    registry.UpdatePopulation(1, 10, 1000);

    EXPECT_FALSE(registry.SelectShardForLogin(20000).has_value());
}
