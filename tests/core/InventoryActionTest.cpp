#include <gtest/gtest.h>

#include "core/network/InventoryAction.h"

using Engine::Net::DeserializeInventoryAction;
using Engine::Net::InventoryActionCommand;
using Engine::Net::InventoryActionType;
using Engine::Net::NetBuffer;
using Engine::Net::SerializeInventoryAction;

TEST(InventoryActionTest, RoundTripsACombineCommand) {
    InventoryActionCommand sent;
    sent.action = InventoryActionType::Combine;
    sent.slotIndex = 3;
    sent.secondarySlotIndex = 8;
    sent.worldEntityId = 42;

    InventoryActionCommand received;
    ASSERT_TRUE(DeserializeInventoryAction(SerializeInventoryAction(sent), received));
    EXPECT_EQ(received.action, InventoryActionType::Combine);
    EXPECT_EQ(received.slotIndex, 3u);
    EXPECT_EQ(received.secondarySlotIndex, 8u);
    EXPECT_EQ(received.worldEntityId, 42u);
}

TEST(InventoryActionTest, EmptyPayloadFails) {
    InventoryActionCommand received;
    EXPECT_FALSE(DeserializeInventoryAction(NetBuffer{}, received));
}
