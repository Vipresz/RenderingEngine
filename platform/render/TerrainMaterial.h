#pragma once

#include "render/components/TerrainMaterialComponent.h"

#include <memory>
#include <string>
#include <vector>

namespace Engine {
namespace Platform {
namespace Render {

class TerrainMaterialLibrary {
public:
    TerrainMaterialLibrary();
    ~TerrainMaterialLibrary();

    TerrainMaterialLibrary(const TerrainMaterialLibrary&) = delete;
    TerrainMaterialLibrary& operator=(const TerrainMaterialLibrary&) = delete;

    // libraryRoot contains manifest.json, bases/, and regions/.
    bool Load(const std::string& libraryRoot);

    bool Apply(Components::TerrainMaterialComponent& out, const std::string& regionId) const;

    const std::vector<std::string>& RegionIds() const { return m_regionIds; }
    const std::string& Root() const { return m_root; }

    // Walks up from the process working directory looking for assets/library/manifest.json.
    static std::string FindDefaultRoot();

private:
    struct Cache;
    std::unique_ptr<Cache> m_cache;
    std::string m_root;
    std::vector<std::string> m_regionIds;
};

} // namespace Render
} // namespace Platform
} // namespace Engine
