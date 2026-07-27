#include "audio/AudioSystem.h"

#include "audio/NullAudioBackend.h"
#include "Logger.h"

namespace Engine {
namespace Audio {

AudioSystem::AudioSystem() : m_backend(std::make_unique<NullAudioBackend>()) {
    SetPriority(140);
}

AudioSystem::AudioSystem(std::unique_ptr<IAudioBackend> backend) : m_backend(std::move(backend)) {
    SetPriority(140);
}

void AudioSystem::SetBackend(std::unique_ptr<IAudioBackend> backend) {
    if (m_backend) {
        m_backend->Shutdown();
    }
    m_backend = std::move(backend);
    if (m_backend) {
        m_backend->Initialize();
    }
}

void AudioSystem::Initialize(ECS::Registry& registry) {
    (void)registry;
    if (m_backend) {
        m_backend->Initialize();
    }
    LOG_INFO("AudioSystem", "Audio system initialized");
}

SoundHandle AudioSystem::LoadSound(const std::string& path) {
    if (!m_backend) {
        return {};
    }
    const auto it = m_soundCache.find(path);
    if (it != m_soundCache.end()) {
        return it->second;
    }
    const SoundHandle handle = m_backend->LoadSound(path);
    m_soundCache[path] = handle;
    return handle;
}

void AudioSystem::Play2D(const std::string& path, float volume, bool loop) {
    if (!m_backend) {
        return;
    }
    m_backend->Play2D(LoadSound(path), volume, loop);
}

void AudioSystem::Play3D(const std::string& path, const glm::vec3& position, float volume, bool loop) {
    if (!m_backend) {
        return;
    }
    m_backend->Play3D(LoadSound(path), position, volume, loop);
}

void AudioSystem::Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    (void)deltaTime;
    (void)registry;
    (void)tickNumber;
    if (!m_backend) {
        return;
    }

    if (m_cameraSource) {
        glm::vec3 forward(0.0f);
        glm::vec3 right(0.0f);
        if (m_cameraSource->GetMovementBasis(forward, right)) {
            m_backend->SetListener(m_cameraSource->GetEyePosition(), forward, glm::vec3(0.0f, 1.0f, 0.0f));
        } else {
            m_backend->SetListener(m_cameraSource->GetEyePosition(), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        }
    }

    m_backend->Update();
}

void AudioSystem::Shutdown(ECS::Registry& registry) {
    (void)registry;
    if (m_backend) {
        m_backend->Shutdown();
    }
    m_soundCache.clear();
}

} // namespace Audio
} // namespace Engine
