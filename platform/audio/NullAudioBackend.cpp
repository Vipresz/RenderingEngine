#include "audio/NullAudioBackend.h"

namespace Engine {
namespace Audio {

SoundHandle NullAudioBackend::LoadSound(const std::string& path) {
    SoundHandle handle;
    handle.id = m_nextId++;
    m_loadedPaths[handle.id] = path;
    return handle;
}

void NullAudioBackend::UnloadSound(SoundHandle handle) {
    m_loadedPaths.erase(handle.id);
}

void NullAudioBackend::Play2D(SoundHandle handle, float volume, bool loop) {
    (void)handle;
    (void)volume;
    (void)loop;
}

void NullAudioBackend::Play3D(SoundHandle handle, const glm::vec3& position, float volume, bool loop) {
    (void)handle;
    (void)position;
    (void)volume;
    (void)loop;
}

void NullAudioBackend::Stop(SoundHandle handle) {
    (void)handle;
}

void NullAudioBackend::SetListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) {
    (void)position;
    (void)forward;
    (void)up;
}

} // namespace Audio
} // namespace Engine
