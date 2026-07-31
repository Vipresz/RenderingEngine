#include "network/CellLayerAssigner.h"

#include <cmath>

namespace Engine {
namespace Platform {
namespace Networking {

CellLayerAssigner::CellLayerAssigner(const Engine::Net::ScalePolicyConfig& policy) : m_policy(policy) {}

void CellLayerAssigner::SetPolicy(const Engine::Net::ScalePolicyConfig& policy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_policy = policy;
}

CellLayerAssigner::CellKey CellLayerAssigner::ToCell(const glm::vec3& position) const {
    const float cellSize = (std::max)(m_policy.densityCellSize, 1.0f);
    return CellKey{static_cast<int>(std::floor(position.x / cellSize)),
                   static_cast<int>(std::floor(position.y / cellSize)),
                   static_cast<int>(std::floor(position.z / cellSize))};
}

uint8_t CellLayerAssigner::AssignLayerForCell(const CellKey& cell,
                                            size_t cellPopulation,
                                            Engine::Net::UserId userId) {
    if (cellPopulation <= m_policy.cellLayerDensityThreshold) {
        return 0;
    }

    const uint64_t cellHash = static_cast<uint64_t>(CellKeyHash{}(cell));
    m_cellAssignments[cell] = static_cast<uint32_t>(cellPopulation);
    return static_cast<uint8_t>(1u + ((cellHash ^ static_cast<uint64_t>(userId)) % 2u));
}

void CellLayerAssigner::UpdatePlayer(Engine::Net::UserId userId,
                                     const glm::vec3& position,
                                     size_t cellPopulation) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const CellKey cell = ToCell(position);
    m_playerLayers[userId] = AssignLayerForCell(cell, cellPopulation, userId);
}

void CellLayerAssigner::RemovePlayer(Engine::Net::UserId userId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_playerLayers.erase(userId);
}

void CellLayerAssigner::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_playerLayers.clear();
    m_cellAssignments.clear();
}

uint8_t CellLayerAssigner::GetPlayerLayer(Engine::Net::UserId userId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_playerLayers.find(userId);
    if (it == m_playerLayers.end()) {
        return 0;
    }
    return it->second;
}

bool CellLayerAssigner::ShouldIsolateLayers(Engine::Net::UserId observerId,
                                            Engine::Net::UserId sourceId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto observerIt = m_playerLayers.find(observerId);
    const auto sourceIt = m_playerLayers.find(sourceId);
    if (observerIt == m_playerLayers.end() || sourceIt == m_playerLayers.end()) {
        return false;
    }
    return observerIt->second != sourceIt->second;
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
