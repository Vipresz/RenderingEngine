#include "render/AssetManager.h"

namespace Engine {
namespace Platform {
namespace Render {

AssetManager& AssetManager::Instance() {
    static AssetManager instance;
    return instance;
}

std::shared_ptr<Model> AssetManager::LoadModel(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (const auto it = m_models.find(path); it != m_models.end()) {
        if (std::shared_ptr<Model> existing = it->second.lock()) {
            return existing;
        }
    }

    auto model = std::make_shared<Model>();
    model->LoadModel(path);
    if (model->GetMeshCount() == 0) {
        return nullptr;
    }

    m_models[path] = model;
    return model;
}

void AssetManager::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_models.clear();
}

size_t AssetManager::GetCachedModelCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_models.size();
}

} // namespace Render
} // namespace Platform
} // namespace Engine
