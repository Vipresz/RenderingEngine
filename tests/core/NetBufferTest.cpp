#include <gtest/gtest.h>

#include "core/network/NetBuffer.h"

using Engine::Net::NetBuffer;

TEST(NetBufferTest, WriteReadPrimitivesRoundTrip) {
    NetBuffer buffer;
    buffer.WriteU8(42);
    buffer.WriteU16(1000);
    buffer.WriteU32(50000);
    buffer.WriteI32(-12345);
    buffer.WriteFloat(3.14f);
    buffer.WriteString("hello");

    buffer.ResetRead();

    uint8_t u8 = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    int32_t i32 = 0;
    float f = 0.0f;
    std::string str;

    ASSERT_TRUE(buffer.ReadU8(u8));
    ASSERT_TRUE(buffer.ReadU16(u16));
    ASSERT_TRUE(buffer.ReadU32(u32));
    ASSERT_TRUE(buffer.ReadI32(i32));
    ASSERT_TRUE(buffer.ReadFloat(f));
    ASSERT_TRUE(buffer.ReadString(str));

    EXPECT_EQ(u8, 42);
    EXPECT_EQ(u16, 1000);
    EXPECT_EQ(u32, 50000);
    EXPECT_EQ(i32, -12345);
    EXPECT_FLOAT_EQ(f, 3.14f);
    EXPECT_EQ(str, "hello");
}

TEST(NetBufferTest, BlobRoundTrip) {
    NetBuffer buffer;
    const std::vector<uint8_t> blob = {1, 2, 3, 4, 5};
    buffer.WriteBlob(blob);

    buffer.ResetRead();
    std::vector<uint8_t> out;
    ASSERT_TRUE(buffer.ReadBlob(out));
    EXPECT_EQ(out, blob);
}

TEST(NetBufferTest, ReadPastEndFails) {
    NetBuffer buffer;
    buffer.WriteU8(1);
    buffer.ResetRead();

    uint8_t value = 0;
    ASSERT_TRUE(buffer.ReadU8(value));
    EXPECT_FALSE(buffer.ReadU8(value));
}
