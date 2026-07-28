#pragma once

#include "render/Model.h"

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
    void Clear();

    size_t GetCachedModelCount() const;

private:
    AssetManager() = default;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::weak_ptr<Model>> m_models;
};

} // namespace Render
} // namespace Platform
} // namespace Engine
