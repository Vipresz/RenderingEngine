#pragma once

#include "core/network/NetTypes.h"

#include <cmath>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

namespace Engine {
namespace Platform {
namespace Networking {

// Spatial interest management — limits who receives updates based on proximity.
class InterestManager {
public:
    explicit InterestManager(float cellSize = 32.0f, float defaultInterestRadius = 128.0f);

    InterestManager(const InterestManager&) = delete;
    InterestManager& operator=(const InterestManager&) = delete;

    void SetCellSize(float cellSize);
    void SetDefaultInterestRadius(float radius);
    float GetDefaultInterestRadius() const { return m_defaultInterestRadius; }

    void SetMaxObserversPerBroadcast(size_t maxObservers);
    size_t GetMaxObserversPerBroadcast() const { return m_maxObserversPerBroadcast; }
    float GetCellSize() const { return m_cellSize; }

    void UpdateEntity(Engine::Net::UserId userId, const glm::vec3& position);
    void RemoveEntity(Engine::Net::UserId userId);
    void Clear();

    bool TryGetEntityPosition(Engine::Net::UserId userId, glm::vec3& outPosition) const;
    size_t CountUsersInCell(const glm::vec3& position) const;
    uint32_t GetMaxCellDensity() const;

    // Returns user IDs that should receive updates about `sourceUserId` from `sourcePosition`.
    std::vector<Engine::Net::UserId> GetInterestedObservers(Engine::Net::UserId sourceUserId,
                                                            const glm::vec3& sourcePosition) const;

    // Returns user IDs visible from an observer position (for snapshot fan-out).
    std::vector<Engine::Net::UserId> QueryNearbyUsers(const glm::vec3& observerPosition,
                                                      float radius) const;

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
    void RebuildSpatialIndex();

    mutable std::mutex m_mutex;
    float m_cellSize = 32.0f;
    float m_defaultInterestRadius = 128.0f;
    size_t m_maxObserversPerBroadcast = 64;
    std::unordered_map<Engine::Net::UserId, glm::vec3> m_positions;
    std::unordered_map<CellKey, std::unordered_set<Engine::Net::UserId>, CellKeyHash> m_cells;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
