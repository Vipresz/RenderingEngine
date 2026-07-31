#include "render/AssetManager.h"

namespace Engine {
namespace Platform {
namespace Render {

AssetManager& AssetManager::Instance() {
    static AssetManager instance;
    return instance;
}

std::shared_ptr<Model> AssetManager::LoadModelInternal(const std::string& path, bool forceReload) {
    if (path.empty()) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!forceReload) {
        if (const auto it = m_models.find(path); it != m_models.end()) {
            if (std::shared_ptr<Model> existing = it->second.lock()) {
                return existing;
            }
        }
    } else {
        m_models.erase(path);
    }

    auto model = std::make_shared<Model>();
    try {
        model->LoadModel(path);
    } catch (const std::exception&) {
        return nullptr;
    }
    if (model->GetMeshCount() == 0) {
        return nullptr;
    }

    m_models[path] = model;
    m_loadGenerations[path] = m_loadGenerations[path] + 1;
    return model;
}

std::shared_ptr<Model> AssetManager::LoadModel(const std::string& path) {
    return LoadModelInternal(path, false);
}

std::shared_ptr<Model> AssetManager::ReloadModel(const std::string& path) {
    return LoadModelInternal(path, true);
}

bool AssetManager::EvictModel(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    return m_models.erase(path) > 0;
}

std::shared_ptr<Model> AssetManager::GetCachedModel(const std::string& path) const {
    if (path.empty()) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_models.find(path);
    if (it == m_models.end()) {
        return nullptr;
    }
    return it->second.lock();
}

uint64_t AssetManager::GetLoadGeneration(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_loadGenerations.find(path);
    return it == m_loadGenerations.end() ? 0 : it->second;
}

void AssetManager::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_models.clear();
    m_loadGenerations.clear();
}

size_t AssetManager::GetCachedModelCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_models.size();
}

} // namespace Render
} // namespace Platform
} // namespace Engine
