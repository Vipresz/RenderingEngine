#include <gtest/gtest.h>

#include "core/network/VitalsReplication.h"

using Engine::Net::NetBuffer;
using Engine::Net::ReplicatedVitals;

TEST(VitalsReplicationTest, RoundTripsHealthAndMana) {
    ReplicatedVitals sent;
    sent.currentHealth = 37.5f;
    sent.maxHealth = 80.0f;
    sent.currentMana = 12.0f;
    sent.maxMana = 40.0f;

    ReplicatedVitals received;
    ASSERT_TRUE(ReplicatedVitals::Deserialize(sent.Serialize(), received));
    EXPECT_FLOAT_EQ(received.currentHealth, sent.currentHealth);
    EXPECT_FLOAT_EQ(received.maxHealth, sent.maxHealth);
    EXPECT_FLOAT_EQ(received.currentMana, sent.currentMana);
    EXPECT_FLOAT_EQ(received.maxMana, sent.maxMana);
}

TEST(VitalsReplicationTest, TruncatedPayloadFails) {
    NetBuffer buffer;
    buffer.WriteFloat(1.0f);
    ReplicatedVitals received;
    EXPECT_FALSE(ReplicatedVitals::Deserialize(buffer, received));
}
