#include <gtest/gtest.h>

#include "core/network/UdpMessageAuth.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

struct TestUdpHeader {
    uint32_t magic = 0x41574E55u;
    uint16_t version = 1;
    uint16_t type = 40;
    uint64_t sequence = 7;
    uint64_t authTag = 0;
};

} // namespace

TEST(UdpMessageAuthTest, ComputesAndVerifiesMatchingTag) {
    TestUdpHeader header;
    const uint8_t payload[] = {0x01, 0x02, 0x03};

    header.authTag = Engine::Net::ComputeUdpAuthTag(
        0x123456789ABCDEF0ull, &header, offsetof(TestUdpHeader, authTag), payload, sizeof(payload));
    EXPECT_NE(header.authTag, 0u);
    EXPECT_TRUE(Engine::Net::VerifyUdpAuthTag(
        0x123456789ABCDEF0ull, header.authTag, &header, offsetof(TestUdpHeader, authTag), payload, sizeof(payload)));
}

TEST(UdpMessageAuthTest, RejectsTamperedPayload) {
    TestUdpHeader header;
    uint8_t payload[] = {0x01, 0x02, 0x03};

    header.authTag = Engine::Net::ComputeUdpAuthTag(
        0xABCDEF00ull, &header, offsetof(TestUdpHeader, authTag), payload, sizeof(payload));
    payload[2] = 0xFF;

    EXPECT_FALSE(Engine::Net::VerifyUdpAuthTag(
        0xABCDEF00ull, header.authTag, &header, offsetof(TestUdpHeader, authTag), payload, sizeof(payload)));
}

TEST(UdpMessageAuthTest, ZeroSessionTokenProducesNoTag) {
    TestUdpHeader header;
    EXPECT_EQ(Engine::Net::ComputeUdpAuthTag(0, &header, offsetof(TestUdpHeader, authTag), nullptr, 0), 0u);
}
