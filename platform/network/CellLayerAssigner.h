#pragma once

#include "core/network/NetTypes.h"
#include "core/network/ScalePolicyConfig.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>

#include <glm/glm.hpp>

namespace Engine {
namespace Platform {
namespace Networking {

// Assigns in-zone instance layers for overcrowded spatial cells.
class CellLayerAssigner {
public:
    explicit CellLayerAssigner(const Engine::Net::ScalePolicyConfig& policy);

    void SetPolicy(const Engine::Net::ScalePolicyConfig& policy);
    void UpdatePlayer(Engine::Net::UserId userId, const glm::vec3& position, size_t cellPopulation);
    void RemovePlayer(Engine::Net::UserId userId);
    void Clear();

    uint8_t GetPlayerLayer(Engine::Net::UserId userId) const;
    bool ShouldIsolateLayers(Engine::Net::UserId observerId, Engine::Net::UserId sourceId) const;

private:
    struct CellKey {
        int x = 0;
        int y = 0;
        int z = 0;

        bool operator==(const CellKey& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct CellKeyHash {
        size_t operator()(const CellKey& key) const {
            const size_t hx = static_cast<size_t>(key.x) * 73856093u;
            const size_t hy = static_cast<size_t>(key.y) * 19349663u;
            const size_t hz = static_cast<size_t>(key.z) * 83492791u;
            return hx ^ hy ^ hz;
        }
    };

    CellKey ToCell(const glm::vec3& position) const;
    uint8_t AssignLayerForCell(const CellKey& cell, size_t cellPopulation, Engine::Net::UserId userId);

    mutable std::mutex m_mutex;
    Engine::Net::ScalePolicyConfig m_policy;
    std::unordered_map<Engine::Net::UserId, uint8_t> m_playerLayers;
    std::unordered_map<CellKey, uint32_t, CellKeyHash> m_cellAssignments;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
