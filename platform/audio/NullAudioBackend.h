#pragma once

#include "audio/IAudioBackend.h"

#include <unordered_map>

namespace Engine {
namespace Audio {

// No-op backend — safe default until a real audio library is linked.
class NullAudioBackend : public IAudioBackend {
public:
    bool Initialize() override { return true; }
    void Shutdown() override {}
    void Update() override {}

    SoundHandle LoadSound(const std::string& path) override;
    void UnloadSound(SoundHandle handle) override;

    void Play2D(SoundHandle handle, float volume, bool loop) override;
    void Play3D(SoundHandle handle, const glm::vec3& position, float volume, bool loop) override;
    void Stop(SoundHandle handle) override;
    void SetListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) override;

private:
    uint32_t m_nextId = 1;
    std::unordered_map<uint32_t, std::string> m_loadedPaths;
};

} // namespace Audio
} // namespace Engine
