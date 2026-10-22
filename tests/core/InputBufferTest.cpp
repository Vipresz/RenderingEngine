#include <gtest/gtest.h>

#include "core/network/InputBuffer.h"

#include <vector>

using Engine::Net::InputBuffer;
using Engine::Net::MovementCommand;

namespace {

MovementCommand CommandAt(uint64_t tick) {
    MovementCommand command;
    command.tick = tick;
    command.moveX = static_cast<float>(tick);
    return command;
}

} // namespace

TEST(InputBufferTest, GetAtReturnsCommandsInPushOrder) {
    InputBuffer buffer;
    EXPECT_EQ(buffer.Size(), 0u);
    EXPECT_EQ(buffer.GetAt(0), nullptr);

    buffer.Push(CommandAt(1));
    buffer.Push(CommandAt(2));
    ASSERT_EQ(buffer.Size(), 2u);
    ASSERT_NE(buffer.GetAt(0), nullptr);
    EXPECT_EQ(buffer.GetAt(0)->tick, 1u);
    EXPECT_EQ(buffer.GetAt(1)->tick, 2u);
    EXPECT_EQ(buffer.GetAt(2), nullptr);
}

TEST(InputBufferTest, ForEachAfterSkipsCommandsAtOrBeforeTheTick) {
    InputBuffer buffer;
    buffer.Push(CommandAt(4));
    buffer.Push(CommandAt(5));
    buffer.Push(CommandAt(6));

    std::vector<uint64_t> seen;
    buffer.ForEachAfter(5, [&](const MovementCommand& command) { seen.push_back(command.tick); });
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0], 6u);
}

TEST(InputBufferTest, PushPastCapacityDropsTheOldestCommand) {
    InputBuffer buffer;
    for (size_t i = 0; i < InputBuffer::kCapacity; ++i) {
        buffer.Push(CommandAt(i));
    }
    buffer.Push(CommandAt(InputBuffer::kCapacity));

    EXPECT_EQ(buffer.Size(), InputBuffer::kCapacity);
    ASSERT_NE(buffer.GetAt(0), nullptr);
    EXPECT_EQ(buffer.GetAt(0)->tick, 1u);
    EXPECT_EQ(buffer.GetAt(InputBuffer::kCapacity - 1)->tick, InputBuffer::kCapacity);
}

TEST(InputBufferTest, ClearDropsStoredCommands) {
    InputBuffer buffer;
    buffer.Push(CommandAt(1));
    buffer.Clear();
    EXPECT_EQ(buffer.Size(), 0u);
    EXPECT_EQ(buffer.GetAt(0), nullptr);
}
