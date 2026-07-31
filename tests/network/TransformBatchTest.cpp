#include <gtest/gtest.h>

#include "core/network/NetworkPayloads.h"

#include <glm/glm.hpp>

TEST(TransformBatchTest, RoundTripMultipleTransforms) {
    Engine::Net::TransformBatchPayload original;
    for (uint32_t userId = 1; userId <= 3; ++userId) {
        Engine::Net::TransformStatePayload transform;
        transform.userId = userId;
        transform.tick = 99;
        transform.position = glm::vec3(static_cast<float>(userId), 0.0f, 0.0f);
        original.transforms.push_back(transform);
    }

    Engine::Net::TransformBatchPayload decoded;
    ASSERT_TRUE(Engine::Net::TransformBatchPayload::Deserialize(original.Serialize(), decoded));
    ASSERT_EQ(decoded.transforms.size(), 3u);
    EXPECT_EQ(decoded.transforms[1].userId, 2u);
    EXPECT_FLOAT_EQ(decoded.transforms[2].position.x, 3.0f);
}
