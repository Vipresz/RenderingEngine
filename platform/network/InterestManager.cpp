#include "network/InterestManager.h"

#include <algorithm>

namespace Engine {
namespace Platform {
namespace Networking {

InterestManager::InterestManager(float cellSize, float defaultInterestRadius)
    : m_cellSize(std::max(cellSize, 1.0f)), m_defaultInterestRadius(std::max(defaultInterestRadius, 1.0f)) {}

void InterestManager::SetCellSize(float cellSize) {
    m_cellSize = std::max(cellSize, 1.0f);
}

void InterestManager::SetDefaultInterestRadius(float radius) {
    m_defaultInterestRadius = std::max(radius, 1.0f);
}

InterestManager::CellKey InterestManager::ToCell(const glm::vec3& position) const {
    return CellKey{static_cast<int>(std::floor(position.x / m_cellSize)),
                   static_cast<int>(std::floor(position.y / m_cellSize)),
                   static_cast<int>(std::floor(position.z / m_cellSize))};
}

void InterestManager::UpdateEntity(Engine::Net::UserId userId, const glm::vec3& position) {
    const auto existing = m_positions.find(userId);
    if (existing != m_positions.end()) {
        const CellKey oldCell = ToCell(existing->second);
        auto cellIt = m_cells.find(oldCell);
        if (cellIt != m_cells.end()) {
            cellIt->second.erase(userId);
            if (cellIt->second.empty()) {
                m_cells.erase(cellIt);
            }
        }
    }

    m_positions[userId] = position;
    m_cells[ToCell(position)].insert(userId);
}

void InterestManager::RemoveEntity(Engine::Net::UserId userId) {
    const auto existing = m_positions.find(userId);
    if (existing == m_positions.end()) {
        return;
    }

    const CellKey cell = ToCell(existing->second);
    auto cellIt = m_cells.find(cell);
    if (cellIt != m_cells.end()) {
        cellIt->second.erase(userId);
        if (cellIt->second.empty()) {
            m_cells.erase(cellIt);
        }
    }
    m_positions.erase(existing);
}

std::vector<Engine::Net::UserId> InterestManager::QueryNearbyUsers(const glm::vec3& observerPosition,
                                                                   float radius) const {
    std::vector<Engine::Net::UserId> result;
    const float radiusSq = radius * radius;
    const int cellRadius = static_cast<int>(std::ceil(radius / m_cellSize));
    const CellKey center = ToCell(observerPosition);

    for (int dx = -cellRadius; dx <= cellRadius; ++dx) {
        for (int dy = -cellRadius; dy <= cellRadius; ++dy) {
            for (int dz = -cellRadius; dz <= cellRadius; ++dz) {
                const CellKey key{center.x + dx, center.y + dy, center.z + dz};
                const auto cellIt = m_cells.find(key);
                if (cellIt == m_cells.end()) {
                    continue;
                }
                for (Engine::Net::UserId userId : cellIt->second) {
                    const auto posIt = m_positions.find(userId);
                    if (posIt == m_positions.end()) {
                        continue;
                    }
                    const glm::vec3 delta = posIt->second - observerPosition;
                    if (glm::dot(delta, delta) <= radiusSq) {
                        result.push_back(userId);
                    }
                }
            }
        }
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<Engine::Net::UserId> InterestManager::GetInterestedObservers(
    Engine::Net::UserId sourceUserId,
    const glm::vec3& sourcePosition) const {
    std::vector<Engine::Net::UserId> observers = QueryNearbyUsers(sourcePosition, m_defaultInterestRadius);
    observers.erase(std::remove(observers.begin(), observers.end(), sourceUserId), observers.end());
    return observers;
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
