#pragma once

#include "audio/IAudioBackend.h"
#include "core/System.h"
#include "core/ecs/Registry.h"
#include "render/ICameraSource.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace Engine {
namespace Audio {

class AudioSystem : public ECS::ISystem {
public:
    AudioSystem();
    explicit AudioSystem(std::unique_ptr<IAudioBackend> backend);

    void SetBackend(std::unique_ptr<IAudioBackend> backend);
    IAudioBackend* GetBackend() const { return m_backend.get(); }

    void SetCameraSource(Engine::Platform::Render::ICameraSource* camera) { m_cameraSource = camera; }

    SoundHandle LoadSound(const std::string& path);
    void Play2D(const std::string& path, float volume = 1.0f, bool loop = false);
    void Play3D(const std::string& path, const glm::vec3& position, float volume = 1.0f, bool loop = false);

    void Initialize(ECS::Registry& registry) override;
    void Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) override;
    void Shutdown(ECS::Registry& registry) override;

private:
    std::unique_ptr<IAudioBackend> m_backend;
    Engine::Platform::Render::ICameraSource* m_cameraSource = nullptr;
    std::unordered_map<std::string, SoundHandle> m_soundCache;
};

} // namespace Audio
} // namespace Engine
