#pragma once

#include "core/scene/Scene.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {
namespace Scene {

// Reusable entity template for spawning grouped objects.
struct Prefab {
    std::string name;
    std::vector<SceneNode> nodes;
    SceneNodeId rootNodeIndex = 0;
};

class PrefabLibrary {
public:
    void Register(const std::string& name, Prefab prefab);
    const Prefab* Find(const std::string& name) const;

private:
    std::unordered_map<std::string, Prefab> m_prefabs;
};

} // namespace Scene
} // namespace Engine
