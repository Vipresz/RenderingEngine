#include "audio/MiniaudioBackend.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <utility>

namespace Engine {
namespace Audio {

namespace {

void DestroyEngine(ma_engine* engine) {
    if (engine) {
        ma_engine_uninit(engine);
        delete engine;
    }
}

void DestroySound(ma_sound* sound) {
    if (sound) {
        ma_sound_uninit(sound);
        delete sound;
    }
}

} // namespace

struct MiniaudioBackend::LoadedSound {
    std::string path;
    std::unique_ptr<ma_sound, void (*)(ma_sound*)> sound{nullptr, DestroySound};
};

MiniaudioBackend::MiniaudioBackend()
    : m_engine(nullptr, DestroyEngine) {}

MiniaudioBackend::~MiniaudioBackend() {
    Shutdown();
}

bool MiniaudioBackend::Initialize() {
    if (m_engine) {
        return true;
    }

    auto* engine = new ma_engine();
    if (ma_engine_init(nullptr, engine) != MA_SUCCESS) {
        delete engine;
        return false;
    }

    m_engine.reset(engine);
    return true;
}

void MiniaudioBackend::Shutdown() {
    m_sounds.clear();
    m_engine.reset();
}

void MiniaudioBackend::Update() {}

SoundHandle MiniaudioBackend::LoadSound(const std::string& path) {
    SoundHandle handle;
    handle.id = m_nextId++;

    if (!m_engine) {
        return handle;
    }

    auto* sound = new ma_sound();
    if (ma_sound_init_from_file(m_engine.get(), path.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, sound) !=
        MA_SUCCESS) {
        delete sound;
        handle.id = 0;
        return handle;
    }

    auto loaded = std::make_unique<LoadedSound>();
    loaded->path = path;
    loaded->sound.reset(sound);
    m_sounds[handle.id] = std::move(loaded);
    return handle;
}

void MiniaudioBackend::UnloadSound(SoundHandle handle) {
    m_sounds.erase(handle.id);
}

void MiniaudioBackend::Play2D(SoundHandle handle, float volume, bool loop) {
    auto it = m_sounds.find(handle.id);
    if (it == m_sounds.end() || !it->second || !it->second->sound) {
        return;
    }

    ma_sound_set_volume(it->second->sound.get(), volume);
    ma_sound_set_looping(it->second->sound.get(), loop ? MA_TRUE : MA_FALSE);
    ma_sound_seek_to_pcm_frame(it->second->sound.get(), 0);
    ma_sound_start(it->second->sound.get());
}

void MiniaudioBackend::Play3D(SoundHandle handle, const glm::vec3& position, float volume, bool loop) {
    auto it = m_sounds.find(handle.id);
    if (it == m_sounds.end() || !it->second || !it->second->sound) {
        return;
    }

    ma_sound_set_volume(it->second->sound.get(), volume);
    ma_sound_set_looping(it->second->sound.get(), loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_position(it->second->sound.get(), position.x, position.y, position.z);
    ma_sound_seek_to_pcm_frame(it->second->sound.get(), 0);
    ma_sound_start(it->second->sound.get());
}

void MiniaudioBackend::Stop(SoundHandle handle) {
    auto it = m_sounds.find(handle.id);
    if (it == m_sounds.end() || !it->second || !it->second->sound) {
        return;
    }
    ma_sound_stop(it->second->sound.get());
}

void MiniaudioBackend::SetListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) {
    if (!m_engine) {
        return;
    }

    ma_engine_listener_set_position(m_engine.get(), 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(m_engine.get(), 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(m_engine.get(), 0, up.x, up.y, up.z);
}

} // namespace Audio
} // namespace Engine
