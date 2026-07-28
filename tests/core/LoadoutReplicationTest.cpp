#include <gtest/gtest.h>

#include "core/network/LoadoutReplication.h"

using Engine::Net::ReplicatedLoadout;

TEST(LoadoutReplicationTest, RoundTripBodyAndEquipment) {
    ReplicatedLoadout original;
    original.bodyItemId = "body_male_default";
    original.equipment.push_back({static_cast<uint16_t>(1), "helmet_iron"});
    original.equipment.push_back({static_cast<uint16_t>(4), "boots_leather"});

    const Engine::Net::NetBuffer buffer = original.Serialize();
    ReplicatedLoadout decoded;
    ASSERT_TRUE(ReplicatedLoadout::Deserialize(buffer, decoded));
    EXPECT_EQ(decoded.bodyItemId, original.bodyItemId);
    ASSERT_EQ(decoded.equipment.size(), 2u);
    EXPECT_EQ(decoded.equipment[0].itemId, "helmet_iron");
    EXPECT_EQ(decoded.equipment[1].itemId, "boots_leather");
}
