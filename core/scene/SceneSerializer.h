#pragma once

#include "core/scene/Prefab.h"
#include "core/scene/Scene.h"

#include <string>

namespace Engine {
namespace Scene {

// Minimal JSON serializer for scenes/prefabs (no external JSON dependency).
class SceneSerializer {
public:
    static std::string SerializeScene(const Scene& scene);
    static bool DeserializeScene(const std::string& json, Scene& outScene);

    static std::string SerializePrefab(const Prefab& prefab);
    static bool DeserializePrefab(const std::string& json, Prefab& outPrefab);
};

} // namespace Scene
} // namespace Engine
