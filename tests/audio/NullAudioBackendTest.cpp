#include <gtest/gtest.h>

#include "audio/NullAudioBackend.h"

using Engine::Audio::NullAudioBackend;

TEST(NullAudioBackendTest, LoadAndPlayDoNotCrash) {
    NullAudioBackend backend;
    ASSERT_TRUE(backend.Initialize());

    const auto handle = backend.LoadSound("test.wav");
    EXPECT_TRUE(handle.IsValid());

    backend.Play2D(handle, 1.0f, false);
    backend.Play3D(handle, glm::vec3(1.0f, 0.0f, 0.0f), 0.5f, true);
    backend.SetListener(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    backend.Update();
    backend.Stop(handle);
    backend.UnloadSound(handle);
    backend.Shutdown();
}

TEST(NullAudioBackendTest, InvalidHandleIsNotValid) {
    Engine::Audio::SoundHandle handle;
    EXPECT_FALSE(handle.IsValid());
}
