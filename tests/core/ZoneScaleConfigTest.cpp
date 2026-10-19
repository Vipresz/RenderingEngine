#include <gtest/gtest.h>

#include "core/network/ZoneScaleConfig.h"

#include <string>
#include <vector>

using Engine::Net::OverflowPolicy;
using Engine::Net::ZoneScaleConfig;

TEST(ZoneScaleConfigTest, LoadsShardAndScalePolicyFromJson) {
    const char* json = R"({
      "zoneId": "test_zone",
      "shard": {
        "shardId": 9,
        "maxPlayers": 64,
        "redirectHost": "10.0.0.5",
        "redirectTcpPort": 7000,
        "redirectUdpPort": 7001
      },
      "scalePolicy": {
        "overflowPolicy": "ShardFirst",
        "shardSoftCap": 50,
        "shardHardCap": 64,
        "cellLayerDensityThreshold": 20,
        "layerFullRadiusCrowded": 12.0,
        "observersCapCrowded": 16
      }
    })";

    ZoneScaleConfig config;
    ASSERT_TRUE(ZoneScaleConfig::LoadFromJson(json, config));
    EXPECT_EQ(config.zoneId, "test_zone");
    EXPECT_EQ(config.shard.shardId, 9u);
    EXPECT_EQ(config.shard.maxPlayers, 64u);
    EXPECT_EQ(config.shard.redirectHost, "10.0.0.5");
    EXPECT_EQ(config.shard.redirectTcpPort, 7000);
    EXPECT_EQ(config.shard.redirectUdpPort, 7001);
    EXPECT_EQ(config.scalePolicy.overflowPolicy, OverflowPolicy::ShardFirst);
    EXPECT_EQ(config.scalePolicy.shardSoftCap, 50u);
    EXPECT_EQ(config.scalePolicy.shardHardCap, 64u);
    EXPECT_EQ(config.scalePolicy.cellLayerDensityThreshold, 20u);
    EXPECT_FLOAT_EQ(config.scalePolicy.layerFullRadiusCrowded, 12.0f);
    EXPECT_EQ(config.scalePolicy.observersCapCrowded, 16u);
}

TEST(ZoneScaleConfigTest, LoadsBundledJoinLoadConfigFile) {
    ZoneScaleConfig config;
#ifdef ENGINE_CONFIG_DIR
    const std::string path = std::string(ENGINE_CONFIG_DIR) + "/join_load_scale.json";
    ASSERT_TRUE(ZoneScaleConfig::TryLoadFromFile(path, config)) << path;
#else
    const std::vector<std::string> candidates = {
        "config/join_load_scale.json",
        "../config/join_load_scale.json",
        "../../config/join_load_scale.json",
        "../../../config/join_load_scale.json",
    };

    bool loaded = false;
    for (const std::string& candidate : candidates) {
        if (ZoneScaleConfig::TryLoadFromFile(candidate, config)) {
            loaded = true;
            break;
        }
    }
    ASSERT_TRUE(loaded) << "join_load_scale.json not found from test working directory";
#endif
    EXPECT_EQ(config.zoneId, "join_load_arena");
    EXPECT_EQ(config.scalePolicy.shardSoftCap, 100u);
    EXPECT_EQ(config.scalePolicy.cellLayerDensityThreshold, 32u);
    EXPECT_EQ(config.scalePolicy.overflowPolicy, OverflowPolicy::LayerThenShard);
}
