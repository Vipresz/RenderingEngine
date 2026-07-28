#include "core/scene/Prefab.h"

namespace Engine {
namespace Scene {

void PrefabLibrary::Register(const std::string& name, Prefab prefab) {
    m_prefabs[name] = std::move(prefab);
}

const Prefab* PrefabLibrary::Find(const std::string& name) const {
    const auto it = m_prefabs.find(name);
    return it == m_prefabs.end() ? nullptr : &it->second;
}

} // namespace Scene
} // namespace Engine
