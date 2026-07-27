#pragma once

#include "audio/IAudioBackend.h"

#include <memory>
#include <string>
#include <unordered_map>

struct ma_engine;

namespace Engine {
namespace Audio {

class MiniaudioBackend : public IAudioBackend {
public:
    MiniaudioBackend();
    ~MiniaudioBackend() override;

    bool Initialize() override;
    void Shutdown() override;
    void Update() override;

    SoundHandle LoadSound(const std::string& path) override;
    void UnloadSound(SoundHandle handle) override;

    void Play2D(SoundHandle handle, float volume = 1.0f, bool loop = false) override;
    void Play3D(SoundHandle handle, const glm::vec3& position, float volume = 1.0f, bool loop = false) override;
    void Stop(SoundHandle handle) override;
    void SetListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) override;

private:
    struct LoadedSound;

    std::unique_ptr<ma_engine, void (*)(ma_engine*)> m_engine;
    std::unordered_map<uint32_t, std::unique_ptr<LoadedSound>> m_sounds;
    uint32_t m_nextId = 1;
};

} // namespace Audio
} // namespace Engine
