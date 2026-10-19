#pragma once

#include "render/Model.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Engine {
namespace Platform {
namespace Render {

// Path-keyed cache for shared Model instances.
class AssetManager {
public:
    static AssetManager& Instance();

    std::shared_ptr<Model> LoadModel(const std::string& path);
    std::shared_ptr<Model> ReloadModel(const std::string& path);
    bool EvictModel(const std::string& path);
    std::shared_ptr<Model> GetCachedModel(const std::string& path) const;
    uint64_t GetLoadGeneration(const std::string& path) const;

    void Clear();

    size_t GetCachedModelCount() const;

private:
    AssetManager() = default;

    std::shared_ptr<Model> LoadModelInternal(const std::string& path, bool forceReload);

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::weak_ptr<Model>> m_models;
    std::unordered_map<std::string, uint64_t> m_loadGenerations;
};

} // namespace Render
} // namespace Platform
} // namespace Engine
