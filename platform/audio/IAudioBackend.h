#pragma once

#include "audio/AudioTypes.h"

#include <string>

namespace Engine {
namespace Audio {

// Backend interface — swap in OpenAL/miniaudio implementation later.
class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update() = 0;

    virtual SoundHandle LoadSound(const std::string& path) = 0;
    virtual void UnloadSound(SoundHandle handle) = 0;

    virtual void Play2D(SoundHandle handle, float volume = 1.0f, bool loop = false) = 0;
    virtual void Play3D(SoundHandle handle, const glm::vec3& position, float volume = 1.0f, bool loop = false) = 0;
    virtual void Stop(SoundHandle handle) = 0;
    virtual void SetListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) = 0;
};

} // namespace Audio
} // namespace Engine
