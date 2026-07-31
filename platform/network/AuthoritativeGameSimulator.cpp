#include "network/AuthoritativeGameSimulator.h"

#include "Logger.h"
#include "core/ecs/components/PlayerItemOwnershipComponent.h"
#include "core/network/CharacterMovementSimulator.h"
#include "core/network/ChatMessage.h"
#include "core/network/ReplicationTierThrottle.h"
#include "core/network/InventoryAction.h"
#include "core/network/ItemCombineRecipe.h"
#include "core/network/WorldDropReplication.h"
#include "render/components/AnimationComponent.h"
#include "render/components/TransformComponent.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <map>
#include <optional>
#include <unordered_set>
#include <vector>
#include <glm/glm.hpp>

using Engine::Platform::Render::Components::AnimationComponent;
using Engine::Platform::Render::Components::TransformComponent;

namespace Engine {
namespace Platform {
namespace Networking {

AuthoritativeGameSimulator::AuthoritativeGameSimulator(NetServer& server) : m_server(server) {
    RegisterLoadoutBinding();
    RegisterOwnershipBinding();
    RegisterTradeBinding();
    RegisterWorldDropBinding();
    m_replication.SetSnapshotSendHandler([this](const Engine::Net::EntitySnapshot& snapshot) {
        Engine::Net::NetPacket packet = Engine::Net::MakeMessage(
            Engine::Net::MessageType::EntitySnapshot, snapshot.Serialize(), 0);
        auto playerIt = m_players.find(snapshot.entityId);
        if (playerIt != m_players.end()) {
            m_server.BroadcastPacketToInterested(
                packet, snapshot.entityId, playerIt->second.movement.position);
            return;
        }
        const auto dropIt = m_worldDrops.find(snapshot.entityId);
        if (dropIt != m_worldDrops.end()) {
            m_server.BroadcastPacketToInterested(
                packet, snapshot.entityId, dropIt->second.position);
            return;
        }
        m_server.BroadcastPacket(packet);
    });
}

void AuthoritativeGameSimulator::RegisterLoadoutBinding() {
    Engine::Net::ReplicationComponentDescriptor loadoutDesc;
    loadoutDesc.id = Engine::Net::kLoadoutReplicationComponent;
    loadoutDesc.name = "Loadout";
    m_replication.GetRegistry().Register(std::move(loadoutDesc));

    m_replication.RegisterComponentBinding(Systems::ReplicationComponentBinding{
        Engine::Net::kLoadoutReplicationComponent,
        [this](EntityID entity, ECS::Registry& /*registry*/, Engine::Net::NetBuffer& out) {
            const auto userIt = m_entityToUser.find(entity);
            if (userIt == m_entityToUser.end()) {
                return false;
            }
            const auto playerIt = m_players.find(userIt->second);
            if (playerIt == m_players.end() || playerIt->second.loadout.bodyItemId.empty()) {
                return false;
            }
            out = playerIt->second.loadout.Serialize();
            return true;
        },
        [](EntityID /*entity*/, ECS::Registry& /*registry*/, const Engine::Net::NetBuffer& /*in*/) {
            // Server is the publisher; clients apply loadout via their own bindings.
            return true;
        }});
}

void AuthoritativeGameSimulator::RegisterOwnershipBinding() {
    Engine::Net::ReplicationComponentDescriptor ownershipDesc;
    ownershipDesc.id = Engine::Net::kItemOwnershipReplicationComponent;
    ownershipDesc.name = "ItemOwnership";
    m_replication.GetRegistry().Register(std::move(ownershipDesc));

    m_replication.RegisterComponentBinding(Systems::ReplicationComponentBinding{
        Engine::Net::kItemOwnershipReplicationComponent,
        [this](EntityID entity, ECS::Registry& /*registry*/, Engine::Net::NetBuffer& out) {
            const auto userIt = m_entityToUser.find(entity);
            if (userIt == m_entityToUser.end()) {
                return false;
            }
            const auto playerIt = m_players.find(userIt->second);
            if (playerIt == m_players.end() ||
                playerIt->second.ownership.ownerUserId == Engine::Net::kInvalidUserId) {
                return false;
            }
            out = playerIt->second.ownership.Serialize();
            return true;
        },
        [](EntityID /*entity*/, ECS::Registry& /*registry*/, const Engine::Net::NetBuffer& /*in*/) {
            return true;
        }});
}

bool AuthoritativeGameSimulator::LoadHeightmap(const std::string& imagePath,
                                               float sizeX,
                                               float sizeZ,
                                               float heightScale) {
    m_heightmap = std::make_unique<Engine::Terrain::HeightmapTerrain>();
    if (!m_heightmap->LoadFromImage(imagePath, sizeX, sizeZ, heightScale)) {
        m_heightmap.reset();
        return false;
    }

    Engine::Terrain::HeightmapTerrain* terrain = m_heightmap.get();
    m_terrainSampler = [terrain](float x, float z) { return terrain->GetHeightAt(x, z); };

    const float spawnHeight = terrain->GetHeightAt(m_spawnPosition.x, m_spawnPosition.z);
    m_spawnPosition.y = spawnHeight;
    return true;
}

void AuthoritativeGameSimulator::OnClientLoggedIn(
    Engine::Net::UserId userId,
    const std::string& displayName,
    const std::vector<std::pair<std::string, uint32_t>>& starterInventory,
    const Engine::Net::ReplicatedItemOwnership* restoredOwnership) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    PendingLogin login;
    login.userId = userId;
    login.displayName = displayName;
    login.starterInventory = starterInventory;
    if (restoredOwnership) {
        login.restoredOwnership = *restoredOwnership;
        login.restoredOwnership.ownerUserId = userId;
        login.hasRestoredOwnership = true;
    }
    m_pendingLogins.push_back(std::move(login));
}

void AuthoritativeGameSimulator::OnClientDisconnected(Engine::Net::UserId userId) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_pendingDisconnects.push_back(userId);
}

bool AuthoritativeGameSimulator::HandleGenericData(Engine::Net::UserId userId,
                                                   Engine::Net::GenericDataPayload& payload) {
    if (payload.channel == Engine::Net::kMovementCommandChannel) {
        Engine::Net::MovementCommand command;
        if (!Engine::Net::DeserializeMovementCommand(Engine::Net::NetBuffer(payload.data), command)) {
            return true;
        }
        return HandleMovementCommand(userId, command);
    }

    if (payload.channel == Engine::Net::kLoadoutChannel) {
        Engine::Net::ReplicatedLoadout loadout;
        if (!Engine::Net::ReplicatedLoadout::Deserialize(Engine::Net::NetBuffer(payload.data), loadout)) {
            return true;
        }
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_pendingLoadouts.push_back(PendingLoadout{userId, std::move(loadout)});
        return true;
    }

    if (payload.channel == Engine::Net::kTradeChannel) {
        Engine::Net::TradeCommand command;
        if (!Engine::Net::DeserializeTradeCommand(Engine::Net::NetBuffer(payload.data), command)) {
            return true;
        }
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_pendingTradeCommands.push_back(PendingTradeCommand{userId, std::move(command)});
        return true;
    }

    if (payload.channel == Engine::Net::kChatChannel) {
        Engine::Net::ChatCommand command;
        if (!Engine::Net::DeserializeChatCommand(Engine::Net::NetBuffer(payload.data), command)) {
            return true;
        }
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_pendingChatCommands.push_back(PendingChatCommand{userId, std::move(command)});
        return true;
    }

    if (payload.channel == Engine::Net::kItemOwnershipChannel) {
        Engine::Net::ReplicatedItemOwnership bag;
        if (!Engine::Net::ReplicatedItemOwnership::Deserialize(
                Engine::Net::NetBuffer(payload.data), bag)) {
            return true;
        }
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_pendingOwnership.push_back(PendingOwnership{userId, std::move(bag)});
        return true;
    }

    if (payload.channel == Engine::Net::kInventoryActionChannel) {
        Engine::Net::InventoryActionCommand command;
        if (!Engine::Net::DeserializeInventoryAction(Engine::Net::NetBuffer(payload.data), command)) {
            return true;
        }
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_pendingInventoryActions.push_back(PendingInventoryAction{userId, command});
        return true;
    }

    if (payload.channel == Engine::Net::kWorldResyncChannel) {
        SendWorldResyncTo(userId);
        return true;
    }

    return false;
}

bool AuthoritativeGameSimulator::HandleMovementCommand(Engine::Net::UserId userId,
                                                     Engine::Net::MovementCommand& command) {
    // Ack immediately on receipt so movement RTT is not queued behind tick drain work.
    m_server.SendMovementAck(userId, command.tick);
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_pendingCommands.push_back(PendingCommand{userId, command});
    return true;
}

void AuthoritativeGameSimulator::DrainQueues() {
    std::deque<PendingLogin> logins;
    std::deque<Engine::Net::UserId> disconnects;
    std::deque<PendingCommand> commands;
    std::deque<PendingLoadout> loadouts;
    std::deque<PendingTradeCommand> tradeCommands;
    std::deque<PendingChatCommand> chatCommands;
    std::deque<PendingOwnership> ownershipUpdates;
    std::deque<PendingInventoryAction> inventoryActions;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        logins.swap(m_pendingLogins);
        disconnects.swap(m_pendingDisconnects);
        commands.swap(m_pendingCommands);
        loadouts.swap(m_pendingLoadouts);
        tradeCommands.swap(m_pendingTradeCommands);
        chatCommands.swap(m_pendingChatCommands);
        ownershipUpdates.swap(m_pendingOwnership);
        inventoryActions.swap(m_pendingInventoryActions);
    }

    for (const PendingLogin& login : logins) {
        SpawnPlayer(login.userId, login.displayName);
        if (login.hasRestoredOwnership) {
            ApplyOwnership(login.userId, login.restoredOwnership);
        } else {
            for (const std::pair<std::string, uint32_t>& grant : login.starterInventory) {
                GrantOwnedItem(login.userId, grant.first, grant.second);
            }
        }
        FlushDirtySnapshotForUser(login.userId);
    }
    for (Engine::Net::UserId userId : disconnects) {
        CancelTrade(userId);
        DespawnPlayer(userId);
    }
    for (PendingLoadout& entry : loadouts) {
        ApplyLoadout(entry.userId, std::move(entry.loadout));
    }
    for (PendingTradeCommand& entry : tradeCommands) {
        ApplyTradeCommand(entry.userId, std::move(entry.command));
    }
    for (PendingChatCommand& entry : chatCommands) {
        ApplyChatMessage(entry.userId, std::move(entry.command));
    }
    for (PendingOwnership& entry : ownershipUpdates) {
        ApplyClientBagLayout(entry.userId, std::move(entry.ownership));
    }
    for (PendingInventoryAction& entry : inventoryActions) {
        ApplyInventoryAction(entry.userId, entry.command);
    }

    std::vector<Engine::Net::TransformStatePayload> peerEnqueuePayloads;
    peerEnqueuePayloads.reserve(commands.size());

    for (PendingCommand& entry : commands) {
        if (!ApplyMovementSimulation(entry.userId, entry.command)) {
            continue;
        }
        ++m_tickMovementCommandsProcessed;

        const auto playerIt = m_players.find(entry.userId);
        if (playerIt == m_players.end()) {
            continue;
        }

        Engine::Net::TransformStatePayload payload =
            BuildTransformPayload(entry.userId, playerIt->second, entry.command.tick);
        m_server.StoreAuthoritativeTransform(entry.userId, payload);
        m_server.EnqueueSelfTransform(entry.userId, payload);
        peerEnqueuePayloads.push_back(payload);
        EnqueueDeferredVisibilityUpdate(entry.userId);
    }

    for (const Engine::Net::TransformStatePayload& payload : peerEnqueuePayloads) {
        m_server.EnqueueTransformToPeers(payload, payload.userId, Engine::Net::kInvalidUserId);
    }
}

void AuthoritativeGameSimulator::SpawnPlayer(Engine::Net::UserId userId, const std::string& displayName) {
    if (m_players.find(userId) != m_players.end()) {
        return;
    }

    const glm::vec3 spawnPosition = ComputeSpawnPosition(m_nextSpawnIndex++);

    const ECS::Entity entity = m_registry.CreateEntity();
    auto& transform = m_registry.AddComponent<TransformComponent>(entity.id);
    transform.SetPosition(spawnPosition.x, spawnPosition.y, spawnPosition.z);
    transform.SetScale(1.0f);
    m_registry.AddComponent<AnimationComponent>(entity.id);

    PlayerState player;
    player.entity = entity.id;
    player.displayName = displayName;
    player.ownership.ownerUserId = userId;
    player.movement.position = spawnPosition;
    player.movement.groundLevel = spawnPosition.y;
    if (m_terrainSampler) {
        player.movement.groundLevel = m_terrainSampler(spawnPosition.x, spawnPosition.z);
        player.movement.position.y = player.movement.groundLevel;
        transform.SetPosition(player.movement.position.x, player.movement.position.y,
                              player.movement.position.z);
    }

    m_replication.RegisterReplicatedEntity(userId, entity.id);
    m_entityToUser[entity.id] = userId;
    player.snapshotDirty = true;

    const std::vector<Engine::Net::UserId> nearbyUsers =
        m_server.GetInterestManager().QueryNearbyUsers(
            player.movement.position, m_server.GetInterestManager().GetDefaultInterestRadius());
    for (Engine::Net::UserId peerId : nearbyUsers) {
        if (peerId != userId) {
            player.visiblePeers.insert(peerId);
        }
    }

    m_players[userId] = player;

    const Engine::Net::TransformStatePayload spawnTransform =
        BuildTransformPayload(userId, player, m_tickNumber);
    m_server.StoreAuthoritativeTransform(userId, spawnTransform);
    m_server.SendInterestFilteredJoinState(userId, player.movement.position);
    NotifyInterestedPeersOfSpawn(userId, player.movement.position, displayName, spawnTransform);
    SendWorldSnapshotTo(userId, m_tickNumber);

    LOG_INFO_FMT("AuthoritativeGameSimulator", "Spawned player %u (%s)", userId, displayName.c_str());
}

void AuthoritativeGameSimulator::DespawnPlayer(Engine::Net::UserId userId) {
    CancelTrade(userId);

    auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return;
    }

    m_entityToUser.erase(it->second.entity);
    m_replication.UnregisterReplicatedEntity(userId);
    m_registry.DestroyEntity(it->second.entity);
    m_players.erase(it);
}

void AuthoritativeGameSimulator::ApplyLoadout(Engine::Net::UserId userId,
                                              Engine::Net::ReplicatedLoadout loadout) {
    auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return;
    }
    if (loadout.bodyItemId.empty()) {
        return;
    }
    // Basic size guard against abusive equipment lists.
    if (loadout.equipment.size() > 32) {
        loadout.equipment.resize(32);
    }
    it->second.loadout = std::move(loadout);
    it->second.snapshotDirty = true;
}

void AuthoritativeGameSimulator::ApplyOwnership(Engine::Net::UserId userId,
                                                Engine::Net::ReplicatedItemOwnership ownership) {
    auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return;
    }
    if (ownership.ownerUserId == Engine::Net::kInvalidUserId) {
        ownership.ownerUserId = userId;
    }
    if (ownership.ownerUserId != userId) {
        return;
    }
    if (!ValidateBagOwnership(ownership)) {
        LOG_WARNING_FMT(
            "AuthoritativeGameSimulator",
            "Rejected invalid ownership snapshot for user %u",
            userId);
        return;
    }
    it->second.ownership = std::move(ownership);
    it->second.snapshotDirty = true;
}

void AuthoritativeGameSimulator::ApplyClientBagLayout(Engine::Net::UserId userId,
                                                      Engine::Net::ReplicatedItemOwnership ownership) {
    auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return;
    }
    if (ownership.ownerUserId == Engine::Net::kInvalidUserId) {
        ownership.ownerUserId = userId;
    }
    if (ownership.ownerUserId != userId) {
        return;
    }
    if (!ValidateBagOwnership(ownership)) {
        LOG_WARNING_FMT(
            "AuthoritativeGameSimulator",
            "Rejected invalid bag layout from user %u",
            userId);
        it->second.snapshotDirty = true;
        FlushDirtySnapshotForUser(userId);
        return;
    }
    if (!OwnershipMultisetMatches(it->second.ownership, ownership)) {
        LOG_WARNING_FMT(
            "AuthoritativeGameSimulator",
            "Rejected bag mutation from user %u (item totals changed)",
            userId);
        it->second.snapshotDirty = true;
        FlushDirtySnapshotForUser(userId);
        return;
    }
    it->second.ownership = std::move(ownership);
    it->second.snapshotDirty = true;
    FlushDirtySnapshotForUser(userId);
}

void AuthoritativeGameSimulator::ApplyInventoryAction(Engine::Net::UserId userId,
                                                      Engine::Net::InventoryActionCommand command) {
    auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return;
    }

    if (command.action == Engine::Net::InventoryActionType::Pickup) {
        TryPickupWorldDrop(userId, command.worldEntityId);
        return;
    }

    if (command.action == Engine::Net::InventoryActionType::Combine) {
        ApplyItemCombineAction(userId, command.slotIndex, command.secondarySlotIndex);
        return;
    }

    if (command.slotIndex >= m_bagCapacity) {
        return;
    }

    ECS::PlayerItemOwnershipComponent component =
        Engine::Net::ToOwnershipComponent(it->second.ownership);

    const auto stackIt = std::find_if(
        component.ownedItems.begin(),
        component.ownedItems.end(),
        [&](const ECS::OwnedItemStack& stack) {
            return stack.slotIndex == command.slotIndex;
        });
    if (stackIt == component.ownedItems.end()) {
        return;
    }

    if (m_inventoryActionValidator &&
        !m_inventoryActionValidator(command.action, stackIt->itemId)) {
        LOG_WARNING_FMT(
            "AuthoritativeGameSimulator",
            "Rejected inventory action %u on slot %u for user %u",
            static_cast<unsigned>(command.action),
            command.slotIndex,
            userId);
        return;
    }

    bool success = false;
    std::string droppedItemId;
    uint32_t droppedQuantity = 0;
    uint64_t droppedInstanceId = 0;

    if (command.action == Engine::Net::InventoryActionType::Use) {
        success = component.RemoveFromSlot(command.slotIndex, 1, false);
    } else if (command.action == Engine::Net::InventoryActionType::Drop) {
        droppedItemId = stackIt->itemId;
        droppedQuantity = stackIt->quantity;
        droppedInstanceId = stackIt->instanceId;
        success = component.RemoveFromSlot(command.slotIndex, 0, true);
    }

    if (!success) {
        return;
    }

    it->second.ownership = Engine::Net::FromOwnershipComponent(component);
    it->second.snapshotDirty = true;
    FlushDirtySnapshotForUser(userId);

    if (command.action == Engine::Net::InventoryActionType::Drop && !droppedItemId.empty() &&
        droppedQuantity > 0) {
        glm::vec3 dropPosition = it->second.movement.position;
        dropPosition.y = it->second.movement.groundLevel;
        SpawnWorldDrop(dropPosition, droppedItemId, droppedQuantity, droppedInstanceId);
    }
}

uint32_t AuthoritativeGameSimulator::ResolveStackCap(const std::string& itemId) const {
    if (m_stackCapResolver) {
        return m_stackCapResolver(itemId);
    }
    return 20;
}

bool AuthoritativeGameSimulator::ValidateBagOwnership(
    const Engine::Net::ReplicatedItemOwnership& ownership) const {
    if (ownership.items.size() > Engine::ECS::kMaxOwnedItemStacks) {
        return false;
    }

    std::unordered_set<uint16_t> usedSlots;
    std::unordered_set<uint64_t> usedInstances;
    for (const Engine::Net::ReplicatedOwnedItem& item : ownership.items) {
        if (item.itemId.empty() || item.quantity == 0) {
            return false;
        }
        if (item.slotIndex >= m_bagCapacity) {
            return false;
        }
        if (!usedSlots.insert(item.slotIndex).second) {
            return false;
        }
        const uint32_t stackCap = ResolveStackCap(item.itemId);
        if (item.quantity > stackCap) {
            return false;
        }
        if (item.instanceId > 0) {
            if (item.quantity != 1) {
                return false;
            }
            if (!usedInstances.insert(item.instanceId).second) {
                return false;
            }
        }
    }
    return true;
}

bool AuthoritativeGameSimulator::OwnershipMultisetMatches(
    const Engine::Net::ReplicatedItemOwnership& current,
    const Engine::Net::ReplicatedItemOwnership& proposed) const {
    using ItemKey = std::pair<std::string, uint64_t>;
    std::map<ItemKey, uint32_t> currentCounts;
    std::map<ItemKey, uint32_t> proposedCounts;

    for (const Engine::Net::ReplicatedOwnedItem& item : current.items) {
        currentCounts[{item.itemId, item.instanceId}] += item.quantity;
    }
    for (const Engine::Net::ReplicatedOwnedItem& item : proposed.items) {
        proposedCounts[{item.itemId, item.instanceId}] += item.quantity;
    }

    return currentCounts == proposedCounts;
}

bool AuthoritativeGameSimulator::GrantOwnedItem(Engine::Net::UserId userId,
                                                const std::string& itemId,
                                                uint32_t quantity,
                                                uint64_t instanceId) {
    auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return false;
    }

    ECS::PlayerItemOwnershipComponent component =
        Engine::Net::ToOwnershipComponent(it->second.ownership);
    const uint32_t stackCap = ResolveStackCap(itemId);
    if (!component.GrantItem(itemId, quantity, instanceId, stackCap, m_bagCapacity)) {
        return false;
    }
    it->second.ownership = Engine::Net::FromOwnershipComponent(component);
    it->second.snapshotDirty = true;
    return true;
}

bool AuthoritativeGameSimulator::RemoveOwnedItem(Engine::Net::UserId userId,
                                                 const std::string& itemId,
                                                 uint32_t quantity) {
    auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return false;
    }

    ECS::PlayerItemOwnershipComponent component =
        Engine::Net::ToOwnershipComponent(it->second.ownership);
    if (!component.RemoveItem(itemId, quantity)) {
        return false;
    }
    it->second.ownership = Engine::Net::FromOwnershipComponent(component);
    it->second.snapshotDirty = true;
    return true;
}

bool AuthoritativeGameSimulator::RemoveOwnedItemInstance(Engine::Net::UserId userId,
                                                         uint64_t instanceId) {
    auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return false;
    }

    ECS::PlayerItemOwnershipComponent component =
        Engine::Net::ToOwnershipComponent(it->second.ownership);
    if (!component.RemoveInstance(instanceId)) {
        return false;
    }
    it->second.ownership = Engine::Net::FromOwnershipComponent(component);
    it->second.snapshotDirty = true;
    return true;
}

uint32_t AuthoritativeGameSimulator::CountOwnedItem(Engine::Net::UserId userId,
                                                    const std::string& itemId) const {
    const auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return 0;
    }
    return Engine::Net::ToOwnershipComponent(it->second.ownership).CountItem(itemId);
}

bool AuthoritativeGameSimulator::HasOwnedItem(Engine::Net::UserId userId,
                                              const std::string& itemId,
                                              uint32_t quantity) const {
    const auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return false;
    }
    return Engine::Net::ToOwnershipComponent(it->second.ownership).HasItem(itemId, quantity);
}

const Engine::Net::ReplicatedItemOwnership* AuthoritativeGameSimulator::GetOwnedItems(
    Engine::Net::UserId userId) const {
    const auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return nullptr;
    }
    return &it->second.ownership;
}

void AuthoritativeGameSimulator::ApplyMovement(Engine::Net::UserId userId,
                                               Engine::Net::MovementCommand command) {
    if (!ApplyMovementSimulation(userId, command)) {
        return;
    }
    auto playerIt = m_players.find(userId);
    if (playerIt == m_players.end()) {
        return;
    }
    BroadcastAuthoritativeTransform(userId, playerIt->second, command.tick);
    UpdatePeerVisibility(userId, playerIt->second.movement.position);
}

bool AuthoritativeGameSimulator::ApplyMovementSimulation(Engine::Net::UserId userId,
                                                         Engine::Net::MovementCommand& command) {
    auto playerIt = m_players.find(userId);
    if (playerIt == m_players.end()) {
        return false;
    }

    if (command.tick <= playerIt->second.lastProcessedTick) {
        return false;
    }

    Engine::Net::SanitizeMovementCommand(command, m_movementSpeeds);
    playerIt->second.lastProcessedTick = command.tick;

    PlayerState& player = playerIt->second;
    Engine::Net::CharacterMovementSimulator::SimulateStep(
        player.movement, command, m_terrainSampler);

    if (TransformComponent* transform = m_registry.GetComponent<TransformComponent>(player.entity)) {
        transform->position = player.movement.position;
        transform->SetYaw(player.movement.yawDegrees);
    }

    if (AnimationComponent* anim = m_registry.GetComponent<AnimationComponent>(player.entity)) {
        anim->isJumping = player.movement.isJumping;
        switch (player.movement.animationState) {
            case Engine::Net::MovementAnimationState::Walking:
                anim->currentState = AnimationComponent::AnimationState::Walking;
                break;
            case Engine::Net::MovementAnimationState::Running:
                anim->currentState = AnimationComponent::AnimationState::Running;
                break;
            case Engine::Net::MovementAnimationState::Jumping:
                anim->currentState = AnimationComponent::AnimationState::Jumping;
                break;
            case Engine::Net::MovementAnimationState::TurningLeft:
                anim->currentState = AnimationComponent::AnimationState::TurningLeft;
                break;
            case Engine::Net::MovementAnimationState::TurningRight:
                anim->currentState = AnimationComponent::AnimationState::TurningRight;
                break;
            case Engine::Net::MovementAnimationState::Idle:
            default:
                anim->currentState = AnimationComponent::AnimationState::Idle;
                break;
        }
        anim->isRunning = anim->currentState == AnimationComponent::AnimationState::Running;
        anim->airVelocityX = player.movement.airVelocityX;
        anim->airVelocityZ = player.movement.airVelocityZ;
        anim->locomotionSpeed = std::sqrt(
            player.movement.airVelocityX * player.movement.airVelocityX +
            player.movement.airVelocityZ * player.movement.airVelocityZ);
    }

    return true;
}

void AuthoritativeGameSimulator::FlushDirtySnapshotForUser(Engine::Net::UserId userId) {
    auto it = m_players.find(userId);
    if (it == m_players.end() || !it->second.snapshotDirty) {
        return;
    }

    const Engine::Net::EntitySnapshot snapshot =
        m_replication.BuildEntitySnapshot(userId, m_registry, m_tickNumber);
    if (snapshot.components.empty()) {
        it->second.snapshotDirty = false;
        return;
    }

    Engine::Net::NetPacket packet =
        Engine::Net::MakeMessage(Engine::Net::MessageType::EntitySnapshot, snapshot.Serialize(), 0);
    m_server.BroadcastPacketToInterested(packet, userId, it->second.movement.position);
    m_server.SendPacketToClient(userId, packet);
    it->second.snapshotDirty = false;
    ++m_tickDirtySnapshotsSent;
}

void AuthoritativeGameSimulator::Update(float deltaTime) {
    const auto tickStart = std::chrono::steady_clock::now();
    m_tickMovementCommandsProcessed = 0;
    m_tickDirtySnapshotsSent = 0;
    m_tickVisibilityEvents = 0;
    m_tickPeerTransformsSent = 0;

    DrainQueues();
    ProcessDeferredVisibilityUpdates();

    const Engine::Net::ScalePolicyConfig& policy = m_server.GetScalePolicyConfig();
    const uint32_t cellDensity = m_server.GetInterestManager().GetMaxCellDensity();
    const bool layeringActive =
        policy.IsLayeringActive(static_cast<uint32_t>(m_players.size()), cellDensity);
    for (auto& entry : m_players) {
        m_server.UpdateCellLayerForPlayer(entry.first, entry.second.movement.position);
    }

    m_snapshotAccumulator += deltaTime;
    if (m_snapshotAccumulator >= m_snapshotIntervalSeconds) {
        m_snapshotAccumulator = 0.0f;
        ++m_tickNumber;
        m_server.SetReplicationTickNumber(m_tickNumber);

        for (auto& entry : m_players) {
            if (!entry.second.snapshotDirty) {
                continue;
            }

            Engine::Net::ReplicationTier snapshotTier = Engine::Net::ReplicationTier::Full;
            if (layeringActive && m_server.GetCellLayerAssigner().GetPlayerLayer(entry.first) > 0) {
                snapshotTier = Engine::Net::ReplicationTier::Mid;
            }
            const float playerInterval = Engine::Net::SnapshotIntervalForTier(
                snapshotTier,
                policy.snapshotIntervalNormalSeconds,
                policy.snapshotIntervalCrowdedSeconds,
                policy.snapshotIntervalStressedSeconds);
            entry.second.snapshotAccumulator += m_snapshotIntervalSeconds;
            if (entry.second.snapshotAccumulator < playerInterval) {
                continue;
            }
            entry.second.snapshotAccumulator = 0.0f;

            const Engine::Net::EntitySnapshot snapshot =
                m_replication.BuildEntitySnapshot(entry.first, m_registry, m_tickNumber);
            if (snapshot.components.empty()) {
                entry.second.snapshotDirty = false;
                continue;
            }
            Engine::Net::NetPacket packet = Engine::Net::MakeMessage(
                Engine::Net::MessageType::EntitySnapshot, snapshot.Serialize(), 0);
            m_server.BroadcastPacketToInterested(
                packet, entry.first, entry.second.movement.position);
            m_server.SendPacketToClient(entry.first, packet);
            entry.second.snapshotDirty = false;
            ++m_tickDirtySnapshotsSent;
        }
    } else {
        m_server.SetReplicationTickNumber(m_tickNumber);
    }

    for (auto& entry : m_players) {
        if (entry.second.snapshotDirty) {
            continue;
        }
        Engine::Net::ReplicatedPlayerTrade& trade = entry.second.trade;
        if (trade.sessionId == 0) {
            continue;
        }
        const auto phase = static_cast<ECS::TradePhase>(trade.phase);
        if (phase == ECS::TradePhase::Cancelled || phase == ECS::TradePhase::Completed ||
            phase == ECS::TradePhase::None) {
            trade = {};
        }
    }

    const size_t replicationBudget = m_server.GetScalePolicyConfig().maxPeerTransformsPerTick;
    m_tickPeerTransformsSent =
        static_cast<uint32_t>(m_server.FlushPendingPeerTransforms(replicationBudget));

    m_lastTickMetrics.tickMs =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - tickStart).count();
    m_lastTickMetrics.tickMsMax =
        (std::max)(m_lastTickMetrics.tickMsMax, m_lastTickMetrics.tickMs);
    m_sessionMetrics.sessionTickMsMax =
        (std::max)(m_sessionMetrics.sessionTickMsMax, m_lastTickMetrics.tickMs);
    m_lastTickMetrics.connectedPlayers = static_cast<uint32_t>(m_players.size());
    m_lastTickMetrics.movementCommandsProcessed = m_tickMovementCommandsProcessed;
    m_lastTickMetrics.dirtySnapshotsSent = m_tickDirtySnapshotsSent;
    m_lastTickMetrics.visibilityEvents = m_tickVisibilityEvents;
    m_lastTickMetrics.movementCommandsDropped = m_server.ConsumeMovementCommandsDropped();
    m_lastTickMetrics.peerTransformsSent = m_tickPeerTransformsSent;
    m_lastTickMetrics.replicationQueueDepth =
        static_cast<uint32_t>(m_server.GetReplicationQueueDepth());
    m_lastTickMetrics.peerTransformsDeferred = m_lastTickMetrics.replicationQueueDepth;
    m_lastTickMetrics.tickNumber = m_tickNumber;

    RefreshScalePolicy();

    const Engine::Net::LayerSplitPlan splitPlan = m_server.GetLayerSplitPlan();
    m_lastTickMetrics.shardRedirectRecommended = splitPlan.shouldSplit;
    if (splitPlan.shouldSplit && m_server.GetShardConfig().HasOverflowTarget()) {
        m_server.TryExecuteLayerSplit([this](Engine::Net::UserId userId) {
            OnClientDisconnected(userId);
        });
    }

    m_sessionMetrics.connectedPlayers = m_lastTickMetrics.connectedPlayers;
    m_sessionMetrics.movementCommandsProcessed += m_tickMovementCommandsProcessed;
    m_sessionMetrics.dirtySnapshotsSent += m_tickDirtySnapshotsSent;
    m_sessionMetrics.visibilityEvents += m_tickVisibilityEvents;
    m_sessionMetrics.movementCommandsDropped += m_lastTickMetrics.movementCommandsDropped;
    m_sessionMetrics.sessionPeerTransformsSent += m_tickPeerTransformsSent;
    m_sessionMetrics.sessionPeerTransformsDeferred = m_lastTickMetrics.replicationQueueDepth;
    m_sessionMetrics.tickNumber = m_tickNumber;
}

glm::vec3 AuthoritativeGameSimulator::ComputeSpawnPosition(size_t spawnIndex) const {
    const Engine::Net::ScalePolicyConfig& policy = m_server.GetScalePolicyConfig();
    const float cellSize = m_server.GetInterestManager().GetCellSize();
    const uint32_t densityTarget = std::max(policy.targetMaxCellDensity, 1u);
    const float spacing = (std::clamp)(cellSize / std::sqrt(static_cast<float>(densityTarget)),
                                     policy.spawnSpacingMin,
                                     policy.spawnSpacingMax);

    const size_t populationHint = (std::max)(m_players.size(), spawnIndex + 1);
    const int gridWidth =
        (std::max)(10, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(populationHint)))));

    const int row = static_cast<int>(spawnIndex / static_cast<size_t>(gridWidth));
    const int col = static_cast<int>(spawnIndex % static_cast<size_t>(gridWidth));
    glm::vec3 position = m_spawnPosition;
    position.x += static_cast<float>(col) * spacing;
    position.z += static_cast<float>(row) * spacing;
    if (m_terrainSampler) {
        position.y = m_terrainSampler(position.x, position.z);
    }
    return position;
}

float AuthoritativeGameSimulator::ComputeP95(const std::deque<float>& samples) {
    if (samples.empty()) {
        return 0.0f;
    }
    std::vector<float> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    const size_t index =
        std::min(sorted.size() - 1, static_cast<size_t>(std::ceil(sorted.size() * 0.95) - 1));
    return sorted[index];
}

void AuthoritativeGameSimulator::RefreshScalePolicy() {
    const Engine::Net::ScalePolicyConfig& policy = m_server.GetScalePolicyConfig();
    const uint32_t cellDensity = m_server.GetInterestManager().GetMaxCellDensity();
    const uint32_t players = static_cast<uint32_t>(m_players.size());
    const bool layeringActive = policy.IsLayeringActive(players, cellDensity);
    m_server.ApplyLayeringState(layeringActive);

    m_tickMsHistory.push_back(m_lastTickMetrics.tickMs);
    constexpr size_t kTickHistorySize = 60;
    while (m_tickMsHistory.size() > kTickHistorySize) {
        m_tickMsHistory.pop_front();
    }

    const float tickP95Ms = ComputeP95(m_tickMsHistory);
    m_server.SetObservedTickP95Ms(tickP95Ms);

    float snapshotInterval = policy.snapshotIntervalNormalSeconds;
    if (layeringActive) {
        snapshotInterval = policy.snapshotIntervalCrowdedSeconds;
    }
    if (tickP95Ms >= policy.shardTickMsSoftLimit) {
        snapshotInterval = std::max(snapshotInterval, policy.snapshotIntervalStressedSeconds);
    }
    m_snapshotIntervalSeconds = snapshotInterval;

    m_lastTickMetrics.tickP95Ms = tickP95Ms;
    m_lastTickMetrics.cellDensityMax = cellDensity;
    m_lastTickMetrics.layeringActive = layeringActive;
    m_lastTickMetrics.shardRedirectRecommended =
        Engine::Net::PlanLayerSplit(players, policy).shouldSplit;
}

void AuthoritativeGameSimulator::NotifyInterestedPeersOfSpawn(
    Engine::Net::UserId userId,
    const glm::vec3& spawnPosition,
    const std::string& displayName,
    const Engine::Net::TransformStatePayload& spawnTransform) {
    const std::vector<Engine::Net::UserId> interestedObservers =
        m_server.GetInterestManager().GetInterestedObservers(userId, spawnPosition);
    const size_t visibilityBudget =
        m_server.GetScalePolicyConfig().maxVisibilityEventsPerTick;

    Engine::Net::PeerEventPayload joined;
    joined.userId = userId;
    joined.displayName = displayName;
    const Engine::Net::NetPacket joinedPacket =
        Engine::Net::MakeMessage(Engine::Net::MessageType::PeerJoined, joined.Serialize(), 0);

    for (Engine::Net::UserId observerId : interestedObservers) {
        if (observerId == userId) {
            continue;
        }
        if (m_tickVisibilityEvents >= visibilityBudget) {
            break;
        }

        auto observerIt = m_players.find(observerId);
        if (observerIt == m_players.end()) {
            continue;
        }

        if (observerIt->second.visiblePeers.insert(userId).second) {
            m_server.SendPacketToClient(observerId, joinedPacket);
            ++m_tickVisibilityEvents;
        }
    }

    m_server.SendTransformToInterested(spawnTransform, userId, Engine::Net::kInvalidUserId);
}

void AuthoritativeGameSimulator::BroadcastAuthoritativeTransform(Engine::Net::UserId userId,
                                                                 const PlayerState& player,
                                                                 uint64_t tick) {
    const Engine::Net::TransformStatePayload payload = BuildTransformPayload(userId, player, tick);
    m_server.StoreAuthoritativeTransform(userId, payload);
    m_server.SendTransformToInterested(payload, userId, Engine::Net::kInvalidUserId);
}

void AuthoritativeGameSimulator::SendWorldSnapshotTo(Engine::Net::UserId userId, uint64_t tick) {
    glm::vec3 observerPosition = m_spawnPosition;
    const auto playerIt = m_players.find(userId);
    if (playerIt != m_players.end()) {
        observerPosition = playerIt->second.movement.position;
    }

    const float interestRadius = m_server.GetInterestManager().GetDefaultInterestRadius();
    std::vector<Engine::Net::UserId> nearbyUsers =
        m_server.GetInterestManager().QueryNearbyUsers(observerPosition, interestRadius);

    Engine::Net::WorldSnapshotPayload world;
    world.tick = tick;
    world.entities.reserve(nearbyUsers.size() + m_worldDrops.size());
    for (Engine::Net::UserId entityUserId : nearbyUsers) {
        const auto entryIt = m_players.find(entityUserId);
        if (entryIt == m_players.end()) {
            continue;
        }
        world.entities.push_back(
            m_replication.BuildEntitySnapshot(entityUserId, m_registry, tick));
    }

    for (Engine::Net::NetEntityId dropId : QueryNearbyWorldDrops(observerPosition, interestRadius)) {
        world.entities.push_back(m_replication.BuildEntitySnapshot(dropId, m_registry, tick));
    }

    m_server.SendWorldSnapshotTo(userId, world);
}

void AuthoritativeGameSimulator::SendWorldResyncTo(Engine::Net::UserId userId) {
    SendWorldSnapshotTo(userId, m_tickNumber);
}

void AuthoritativeGameSimulator::EnqueueDeferredVisibilityUpdate(Engine::Net::UserId observerId) {
    if (m_deferredVisibilityPending.insert(observerId).second) {
        m_deferredVisibilityUpdates.push_back(observerId);
    }
}

void AuthoritativeGameSimulator::ProcessDeferredVisibilityUpdates() {
    const size_t visibilityBudget = m_server.GetScalePolicyConfig().maxVisibilityEventsPerTick;
    while (!m_deferredVisibilityUpdates.empty()) {
        if (m_tickVisibilityEvents >= visibilityBudget) {
            break;
        }

        const Engine::Net::UserId observerId = m_deferredVisibilityUpdates.front();
        m_deferredVisibilityUpdates.pop_front();
        m_deferredVisibilityPending.erase(observerId);

        const auto playerIt = m_players.find(observerId);
        if (playerIt == m_players.end()) {
            continue;
        }
        UpdatePeerVisibility(observerId, playerIt->second.movement.position);
    }
}

void AuthoritativeGameSimulator::UpdatePeerVisibility(Engine::Net::UserId observerId,
                                                      const glm::vec3& observerPosition) {
    auto observerIt = m_players.find(observerId);
    if (observerIt == m_players.end()) {
        return;
    }

    const std::vector<Engine::Net::UserId> nearbyUsers =
        m_server.GetInterestManager().QueryNearbyUsers(
            observerPosition, m_server.GetInterestManager().GetDefaultInterestRadius());

    std::unordered_set<Engine::Net::UserId> nextVisible;
    nextVisible.reserve(nearbyUsers.size());
    for (Engine::Net::UserId peerId : nearbyUsers) {
        if (peerId != observerId) {
            nextVisible.insert(peerId);
        }
    }

    PlayerState& observer = observerIt->second;
    for (Engine::Net::UserId peerId : nextVisible) {
        if (observer.visiblePeers.find(peerId) != observer.visiblePeers.end()) {
            continue;
        }
        const auto peerIt = m_players.find(peerId);
        if (peerIt == m_players.end()) {
            continue;
        }

        Engine::Net::PeerEventPayload joined;
        joined.userId = peerId;
        joined.displayName = peerIt->second.displayName;
        m_server.SendPacketToClient(
            observerId,
            Engine::Net::MakeMessage(Engine::Net::MessageType::PeerJoined, joined.Serialize(), 0));

        const Engine::Net::TransformStatePayload transform =
            BuildTransformPayload(peerId, peerIt->second, m_tickNumber);
        if (!m_server.HasUdpEndpointFor(observerId)) {
            m_server.SendPacketToClient(
                observerId,
                Engine::Net::MakeMessage(Engine::Net::MessageType::TransformState, transform.Serialize(), 0));
        }
        ++m_tickVisibilityEvents;
    }

    for (Engine::Net::UserId peerId : observer.visiblePeers) {
        if (nextVisible.find(peerId) != nextVisible.end()) {
            continue;
        }
        Engine::Net::PeerEventPayload left;
        left.userId = peerId;
        const auto peerIt = m_players.find(peerId);
        if (peerIt != m_players.end()) {
            left.displayName = peerIt->second.displayName;
        }
        m_server.SendPacketToClient(
            observerId,
            Engine::Net::MakeMessage(Engine::Net::MessageType::PeerLeft, left.Serialize(), 0));
        ++m_tickVisibilityEvents;
    }

    observer.visiblePeers = std::move(nextVisible);
}

Engine::Net::TransformStatePayload AuthoritativeGameSimulator::BuildTransformPayload(
    Engine::Net::UserId userId,
    const PlayerState& player,
    uint64_t tick) const {
    Engine::Net::TransformStatePayload payload;
    payload.userId = userId;
    payload.tick = tick;
    payload.position = player.movement.position;
    payload.rotationDegrees = glm::vec3(0.0f, player.movement.yawDegrees, 0.0f);
    payload.isJumping = player.movement.isJumping;
    payload.jumpAnimationActive = player.movement.jumpAnimationActive;
    payload.jumpVelocity = player.movement.jumpVelocity;
    payload.airVelocityX = player.movement.airVelocityX;
    payload.airVelocityZ = player.movement.airVelocityZ;
    payload.groundLevel = player.movement.groundLevel;
    payload.animationState = static_cast<uint8_t>(player.movement.animationState);
    return payload;
}

void AuthoritativeGameSimulator::RegisterTradeBinding() {
    Engine::Net::ReplicationComponentDescriptor tradeDesc;
    tradeDesc.id = Engine::Net::kTradeReplicationComponent;
    tradeDesc.name = "Trade";
    m_replication.GetRegistry().Register(std::move(tradeDesc));

    m_replication.RegisterComponentBinding(Systems::ReplicationComponentBinding{
        Engine::Net::kTradeReplicationComponent,
        [this](EntityID entity, ECS::Registry& /*registry*/, Engine::Net::NetBuffer& out) {
            const auto userIt = m_entityToUser.find(entity);
            if (userIt == m_entityToUser.end()) {
                return false;
            }
            const auto playerIt = m_players.find(userIt->second);
            if (playerIt == m_players.end() || playerIt->second.trade.sessionId == 0) {
                return false;
            }
            out = playerIt->second.trade.Serialize();
            return true;
        },
        [](EntityID /*entity*/, ECS::Registry& /*registry*/, const Engine::Net::NetBuffer& /*in*/) {
            return true;
        }});
}

void AuthoritativeGameSimulator::RegisterWorldDropBinding() {
    Engine::Net::ReplicationComponentDescriptor dropDesc;
    dropDesc.id = Engine::Net::kWorldDropReplicationComponent;
    dropDesc.name = "WorldDrop";
    m_replication.GetRegistry().Register(std::move(dropDesc));

    m_replication.RegisterComponentBinding(Systems::ReplicationComponentBinding{
        Engine::Net::kWorldDropReplicationComponent,
        [this](EntityID entity, ECS::Registry& /*registry*/, Engine::Net::NetBuffer& out) {
            const auto netIt = m_entityToWorldDrop.find(entity);
            if (netIt == m_entityToWorldDrop.end()) {
                return false;
            }
            const auto dropIt = m_worldDrops.find(netIt->second);
            if (dropIt == m_worldDrops.end()) {
                return false;
            }
            Engine::Net::ReplicatedWorldDrop drop;
            drop.itemId = dropIt->second.itemId;
            drop.quantity = dropIt->second.quantity;
            drop.instanceId = dropIt->second.instanceId;
            out = drop.Serialize();
            return true;
        },
        [](EntityID /*entity*/, ECS::Registry& /*registry*/, const Engine::Net::NetBuffer& /*in*/) {
            return true;
        }});
}

Engine::Net::NetEntityId AuthoritativeGameSimulator::SpawnWorldDrop(const glm::vec3& position,
                                                                  const std::string& itemId,
                                                                  uint32_t quantity,
                                                                  uint64_t instanceId) {
    if (itemId.empty() || quantity == 0) {
        return Engine::Net::kInvalidNetEntity;
    }

    const Engine::Net::NetEntityId netEntityId = m_nextWorldDropId++;
    const ECS::Entity entity = m_registry.CreateEntity();
    auto& transform = m_registry.AddComponent<TransformComponent>(entity.id);
    transform.SetPosition(position.x, position.y, position.z);
    transform.SetScale(0.35f);

    WorldDropState drop;
    drop.entity = entity.id;
    drop.itemId = itemId;
    drop.quantity = quantity;
    drop.instanceId = instanceId;
    drop.position = position;
    m_worldDrops.emplace(netEntityId, drop);
    m_entityToWorldDrop[entity.id] = netEntityId;
    m_replication.RegisterReplicatedEntity(netEntityId, entity.id);

    const Engine::Net::EntitySnapshot snapshot =
        m_replication.BuildEntitySnapshot(netEntityId, m_registry, m_tickNumber);
    if (!snapshot.components.empty()) {
        Engine::Net::NetPacket packet = Engine::Net::MakeMessage(
            Engine::Net::MessageType::EntitySnapshot, snapshot.Serialize(), 0);
        m_server.BroadcastPacketToInterested(packet, netEntityId, position);
    }
    return netEntityId;
}

void AuthoritativeGameSimulator::BroadcastWorldDropRemoved(Engine::Net::NetEntityId entityId) {
    Engine::Net::PeerEventPayload left;
    left.userId = entityId;
    left.displayName = "WorldDrop";
    m_server.BroadcastPacket(Engine::Net::MakeMessage(
        Engine::Net::MessageType::PeerLeft, left.Serialize(), 0));
}

void AuthoritativeGameSimulator::RemoveWorldDrop(Engine::Net::NetEntityId entityId) {
    const auto dropIt = m_worldDrops.find(entityId);
    if (dropIt == m_worldDrops.end()) {
        return;
    }

    m_entityToWorldDrop.erase(dropIt->second.entity);
    m_replication.UnregisterReplicatedEntity(entityId);
    m_registry.DestroyEntity(dropIt->second.entity);
    m_worldDrops.erase(dropIt);
    BroadcastWorldDropRemoved(entityId);
}

bool AuthoritativeGameSimulator::TryPickupWorldDrop(Engine::Net::UserId userId,
                                                  Engine::Net::NetEntityId entityId) {
    const auto playerIt = m_players.find(userId);
    const auto dropIt = m_worldDrops.find(entityId);
    if (playerIt == m_players.end() || dropIt == m_worldDrops.end()) {
        return false;
    }

    if (m_pickupMaxDistance > 0.0f) {
        const glm::vec3 delta = dropIt->second.position - playerIt->second.movement.position;
        const float horizontalDistanceSq = delta.x * delta.x + delta.z * delta.z;
        if (horizontalDistanceSq > m_pickupMaxDistance * m_pickupMaxDistance) {
            return false;
        }
    }

    if (!GrantOwnedItem(userId, dropIt->second.itemId, dropIt->second.quantity,
                        dropIt->second.instanceId)) {
        return false;
    }

    RemoveWorldDrop(entityId);
    FlushDirtySnapshotForUser(userId);
    return true;
}

bool AuthoritativeGameSimulator::ApplyItemCombineAction(Engine::Net::UserId userId,
                                                        uint16_t primarySlotIndex,
                                                        uint16_t secondarySlotIndex) {
    if (m_combineRecipes.empty()) {
        return false;
    }

    auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return false;
    }
    if (primarySlotIndex >= m_bagCapacity || secondarySlotIndex >= m_bagCapacity) {
        return false;
    }

    ECS::PlayerItemOwnershipComponent component =
        Engine::Net::ToOwnershipComponent(it->second.ownership);

    const ECS::OwnedItemStack* primaryStack = nullptr;
    const ECS::OwnedItemStack* secondaryStack = nullptr;
    for (const ECS::OwnedItemStack& stack : component.ownedItems) {
        if (stack.slotIndex == primarySlotIndex) {
            primaryStack = &stack;
        } else if (stack.slotIndex == secondarySlotIndex) {
            secondaryStack = &stack;
        }
    }
    if (primaryStack == nullptr || secondaryStack == nullptr) {
        return false;
    }

    const std::optional<Engine::Net::ItemCombineRecipe> recipe =
        Engine::Net::FindCombineRecipe(m_combineRecipes, primaryStack->itemId, secondaryStack->itemId);
    if (!recipe.has_value()) {
        return false;
    }

    if (!Engine::Net::ApplyItemCombine(
            component,
            primarySlotIndex,
            secondarySlotIndex,
            *recipe,
            m_stackCapResolver,
            m_bagCapacity)) {
        return false;
    }

    it->second.ownership = Engine::Net::FromOwnershipComponent(component);
    it->second.snapshotDirty = true;
    FlushDirtySnapshotForUser(userId);
    return true;
}

std::vector<Engine::Net::NetEntityId> AuthoritativeGameSimulator::QueryNearbyWorldDrops(
    const glm::vec3& position,
    float radius) const {
    std::vector<Engine::Net::NetEntityId> nearby;
    const float radiusSq = radius * radius;
    for (const auto& entry : m_worldDrops) {
        const glm::vec3 delta = entry.second.position - position;
        const float horizontalDistanceSq = delta.x * delta.x + delta.z * delta.z;
        if (horizontalDistanceSq <= radiusSq) {
            nearby.push_back(entry.first);
        }
    }
    return nearby;
}

bool AuthoritativeGameSimulator::ValidateTradeSessionDistance(
    const Engine::Net::TradeSession& session) const {
    return ArePlayersWithinTradeRange(session.initiatorUserId, session.partnerUserId);
}

bool AuthoritativeGameSimulator::PlayerAvailableForTrade(Engine::Net::UserId userId) const {
    if (m_players.find(userId) == m_players.end()) {
        return false;
    }
    return m_userTradeSession.find(userId) == m_userTradeSession.end();
}

bool AuthoritativeGameSimulator::ArePlayersWithinTradeRange(Engine::Net::UserId firstUserId,
                                                            Engine::Net::UserId secondUserId) const {
    if (m_tradeMaxDistance <= 0.0f) {
        return true;
    }

    const auto firstIt = m_players.find(firstUserId);
    const auto secondIt = m_players.find(secondUserId);
    if (firstIt == m_players.end() || secondIt == m_players.end()) {
        return false;
    }

    const glm::vec3 delta = firstIt->second.movement.position - secondIt->second.movement.position;
    const float horizontalDistanceSq = delta.x * delta.x + delta.z * delta.z;
    return horizontalDistanceSq <= m_tradeMaxDistance * m_tradeMaxDistance;
}

Engine::Net::TradeSession* AuthoritativeGameSimulator::FindTradeSessionFor(Engine::Net::UserId userId) {
    const auto mapping = m_userTradeSession.find(userId);
    if (mapping == m_userTradeSession.end()) {
        return nullptr;
    }
    const auto sessionIt = m_tradeSessions.find(mapping->second);
    if (sessionIt == m_tradeSessions.end()) {
        return nullptr;
    }
    return &sessionIt->second;
}

const Engine::Net::TradeSession* AuthoritativeGameSimulator::FindTradeSessionFor(
    Engine::Net::UserId userId) const {
    const auto mapping = m_userTradeSession.find(userId);
    if (mapping == m_userTradeSession.end()) {
        return nullptr;
    }
    const auto sessionIt = m_tradeSessions.find(mapping->second);
    if (sessionIt == m_tradeSessions.end()) {
        return nullptr;
    }
    return &sessionIt->second;
}

void AuthoritativeGameSimulator::MarkTradeParticipantsDirty(const Engine::Net::TradeSession& session) {
    if (auto initiatorIt = m_players.find(session.initiatorUserId); initiatorIt != m_players.end()) {
        initiatorIt->second.snapshotDirty = true;
    }
    if (auto partnerIt = m_players.find(session.partnerUserId); partnerIt != m_players.end()) {
        partnerIt->second.snapshotDirty = true;
    }
}

void AuthoritativeGameSimulator::SyncTradeStateForSession(const Engine::Net::TradeSession& session) {
    const ECS::PlayerTradeComponent initiatorView =
        Engine::Net::BuildTradeView(session, session.initiatorUserId);
    const ECS::PlayerTradeComponent partnerView =
        Engine::Net::BuildTradeView(session, session.partnerUserId);

    if (auto initiatorIt = m_players.find(session.initiatorUserId); initiatorIt != m_players.end()) {
        initiatorIt->second.trade = Engine::Net::FromTradeView(initiatorView);
        initiatorIt->second.snapshotDirty = true;
    }
    if (auto partnerIt = m_players.find(session.partnerUserId); partnerIt != m_players.end()) {
        partnerIt->second.trade = Engine::Net::FromTradeView(partnerView);
        partnerIt->second.snapshotDirty = true;
    }
}

bool AuthoritativeGameSimulator::EndTradeSession(uint64_t sessionId, ECS::TradePhase finalPhase) {
    const auto sessionIt = m_tradeSessions.find(sessionId);
    if (sessionIt == m_tradeSessions.end()) {
        return false;
    }

    Engine::Net::TradeSession session = sessionIt->second;

    if (finalPhase == ECS::TradePhase::Cancelled) {
        std::optional<ECS::PlayerItemOwnershipComponent> restoredInitiator;
        std::optional<ECS::PlayerItemOwnershipComponent> restoredPartner;

        if (auto initiatorIt = m_players.find(session.initiatorUserId); initiatorIt != m_players.end()) {
            ECS::PlayerItemOwnershipComponent ownership =
                Engine::Net::ToOwnershipComponent(initiatorIt->second.ownership);
            if (!Engine::Net::RestoreOfferToOwnership(
                    ownership, session.initiatorOffer, m_stackCapResolver, m_bagCapacity)) {
                LOG_ERROR_FMT("AuthoritativeGameSimulator",
                              "Failed to restore initiator escrow for trade session %llu",
                              static_cast<unsigned long long>(sessionId));
                return false;
            }
            restoredInitiator = std::move(ownership);
        }
        if (auto partnerIt = m_players.find(session.partnerUserId); partnerIt != m_players.end()) {
            ECS::PlayerItemOwnershipComponent ownership =
                Engine::Net::ToOwnershipComponent(partnerIt->second.ownership);
            if (!Engine::Net::RestoreOfferToOwnership(
                    ownership, session.partnerOffer, m_stackCapResolver, m_bagCapacity)) {
                LOG_ERROR_FMT("AuthoritativeGameSimulator",
                              "Failed to restore partner escrow for trade session %llu",
                              static_cast<unsigned long long>(sessionId));
                return false;
            }
            restoredPartner = std::move(ownership);
        }

        if (restoredInitiator) {
            if (auto initiatorIt = m_players.find(session.initiatorUserId);
                initiatorIt != m_players.end()) {
                initiatorIt->second.ownership =
                    Engine::Net::FromOwnershipComponent(*restoredInitiator);
                initiatorIt->second.snapshotDirty = true;
            }
        }
        if (restoredPartner) {
            if (auto partnerIt = m_players.find(session.partnerUserId); partnerIt != m_players.end()) {
                partnerIt->second.ownership = Engine::Net::FromOwnershipComponent(*restoredPartner);
                partnerIt->second.snapshotDirty = true;
            }
        }
    }

    session.phase = finalPhase;
    SyncTradeStateForSession(session);

    m_userTradeSession.erase(session.initiatorUserId);
    m_userTradeSession.erase(session.partnerUserId);
    m_tradeSessions.erase(sessionIt);
    return true;
}

bool AuthoritativeGameSimulator::TryExecuteTrade(Engine::Net::TradeSession& session) {
    if (!session.BothLocked() || !session.IsOpen()) {
        return false;
    }
    const uint64_t sessionId = session.sessionId;
    if (!ValidateTradeSessionDistance(session)) {
        EndTradeSession(sessionId, ECS::TradePhase::Cancelled);
        return false;
    }

    auto initiatorIt = m_players.find(session.initiatorUserId);
    auto partnerIt = m_players.find(session.partnerUserId);
    if (initiatorIt == m_players.end() || partnerIt == m_players.end()) {
        return false;
    }

    ECS::PlayerItemOwnershipComponent initiatorOwnership =
        Engine::Net::ToOwnershipComponent(initiatorIt->second.ownership);
    ECS::PlayerItemOwnershipComponent partnerOwnership =
        Engine::Net::ToOwnershipComponent(partnerIt->second.ownership);

    if (!Engine::Net::ExecuteEscrowedTradeExchange(initiatorOwnership,
                                                  partnerOwnership,
                                                  session.initiatorOffer,
                                                  session.partnerOffer,
                                                  m_stackCapResolver,
                                                  m_bagCapacity)) {
        return false;
    }

    initiatorIt->second.ownership = Engine::Net::FromOwnershipComponent(initiatorOwnership);
    partnerIt->second.ownership = Engine::Net::FromOwnershipComponent(partnerOwnership);
    initiatorIt->second.snapshotDirty = true;
    partnerIt->second.snapshotDirty = true;

    session.phase = ECS::TradePhase::Completed;
    SyncTradeStateForSession(session);
    EndTradeSession(session.sessionId, ECS::TradePhase::Completed);
    return true;
}

bool AuthoritativeGameSimulator::RequestTrade(Engine::Net::UserId fromUserId,
                                              Engine::Net::UserId toUserId) {
    if (fromUserId == Engine::Net::kInvalidUserId || toUserId == Engine::Net::kInvalidUserId ||
        fromUserId == toUserId) {
        return false;
    }
    if (!PlayerAvailableForTrade(fromUserId) || !PlayerAvailableForTrade(toUserId)) {
        return false;
    }
    if (!ArePlayersWithinTradeRange(fromUserId, toUserId)) {
        return false;
    }

    Engine::Net::TradeSession session;
    session.sessionId = m_nextTradeSessionId++;
    session.initiatorUserId = fromUserId;
    session.partnerUserId = toUserId;
    session.phase = ECS::TradePhase::Requested;

    m_tradeSessions.emplace(session.sessionId, session);
    m_userTradeSession[fromUserId] = session.sessionId;
    m_userTradeSession[toUserId] = session.sessionId;
    SyncTradeStateForSession(session);
    return true;
}

bool AuthoritativeGameSimulator::RespondToTrade(Engine::Net::UserId responderUserId,
                                                Engine::Net::UserId requesterUserId,
                                                bool accept) {
    Engine::Net::TradeSession* session = FindTradeSessionFor(responderUserId);
    if (session == nullptr || session->phase != ECS::TradePhase::Requested ||
        session->initiatorUserId != requesterUserId || session->partnerUserId != responderUserId) {
        return false;
    }

    if (!accept) {
        return EndTradeSession(session->sessionId, ECS::TradePhase::Cancelled);
    }

    if (!ArePlayersWithinTradeRange(responderUserId, requesterUserId)) {
        return false;
    }

    session->phase = ECS::TradePhase::Open;
    SyncTradeStateForSession(*session);
    return true;
}

bool AuthoritativeGameSimulator::CancelTrade(Engine::Net::UserId userId) {
    Engine::Net::TradeSession* session = FindTradeSessionFor(userId);
    if (session == nullptr) {
        return false;
    }
    return EndTradeSession(session->sessionId, ECS::TradePhase::Cancelled);
}

bool AuthoritativeGameSimulator::SetTradeOffer(Engine::Net::UserId userId,
                                               const std::vector<Engine::Net::TradeOfferItem>& offer) {
    Engine::Net::TradeSession* session = FindTradeSessionFor(userId);
    if (session == nullptr || !session->IsOpen()) {
        return false;
    }
    const uint64_t sessionId = session->sessionId;
    if (!ValidateTradeSessionDistance(*session)) {
        EndTradeSession(sessionId, ECS::TradePhase::Cancelled);
        return false;
    }
    if (session->initiatorLocked || session->partnerLocked) {
        return false;
    }
    if (!Engine::Net::IsValidTradeOffer(offer)) {
        return false;
    }

    const auto playerIt = m_players.find(userId);
    if (playerIt == m_players.end()) {
        return false;
    }

    std::vector<Engine::Net::TradeOfferItem>* userOffer = session->OfferFor(userId);
    if (userOffer == nullptr) {
        return false;
    }

    const std::vector<Engine::Net::TradeOfferItem> previousOffer = *userOffer;
    ECS::PlayerItemOwnershipComponent ownership =
        Engine::Net::ToOwnershipComponent(playerIt->second.ownership);
    if (!Engine::Net::ApplyOfferEscrowTransition(
            ownership, previousOffer, offer, m_stackCapResolver, m_bagCapacity)) {
        return false;
    }

    *userOffer = offer;
    playerIt->second.ownership = Engine::Net::FromOwnershipComponent(ownership);
    playerIt->second.snapshotDirty = true;
    SyncTradeStateForSession(*session);
    return true;
}

bool AuthoritativeGameSimulator::LockTradeOffer(Engine::Net::UserId userId) {
    Engine::Net::TradeSession* session = FindTradeSessionFor(userId);
    if (session == nullptr || !session->IsOpen()) {
        return false;
    }
    const uint64_t sessionId = session->sessionId;
    if (!ValidateTradeSessionDistance(*session)) {
        EndTradeSession(sessionId, ECS::TradePhase::Cancelled);
        return false;
    }

    bool* userLock = session->LockFor(userId);
    std::vector<Engine::Net::TradeOfferItem>* userOffer = session->OfferFor(userId);
    if (userLock == nullptr || userOffer == nullptr) {
        return false;
    }

    *userLock = true;
    SyncTradeStateForSession(*session);
    if (session->BothLocked() && !TryExecuteTrade(*session)) {
        session->initiatorLocked = false;
        session->partnerLocked = false;
        SyncTradeStateForSession(*session);
    }
    return true;
}

bool AuthoritativeGameSimulator::UnlockTradeOffer(Engine::Net::UserId userId) {
    Engine::Net::TradeSession* session = FindTradeSessionFor(userId);
    if (session == nullptr || !session->IsOpen()) {
        return false;
    }

    bool* userLock = session->LockFor(userId);
    if (userLock == nullptr) {
        return false;
    }
    *userLock = false;
    SyncTradeStateForSession(*session);
    return true;
}

const Engine::Net::TradeSession* AuthoritativeGameSimulator::GetTradeSessionFor(
    Engine::Net::UserId userId) const {
    return FindTradeSessionFor(userId);
}

void AuthoritativeGameSimulator::ApplyTradeCommand(Engine::Net::UserId userId,
                                                   Engine::Net::TradeCommand command) {
    switch (command.action) {
        case Engine::Net::TradeAction::Request:
            RequestTrade(userId, command.targetUserId);
            break;
        case Engine::Net::TradeAction::Accept:
            RespondToTrade(userId, command.targetUserId, true);
            break;
        case Engine::Net::TradeAction::Decline:
            RespondToTrade(userId, command.targetUserId, false);
            break;
        case Engine::Net::TradeAction::Cancel:
            CancelTrade(userId);
            break;
        case Engine::Net::TradeAction::SetOffer:
            SetTradeOffer(userId, command.offer);
            break;
        case Engine::Net::TradeAction::Lock:
            LockTradeOffer(userId);
            break;
        case Engine::Net::TradeAction::Unlock:
            UnlockTradeOffer(userId);
            break;
        default:
            break;
    }
}

void AuthoritativeGameSimulator::ApplyChatMessage(Engine::Net::UserId userId,
                                                    Engine::Net::ChatCommand command) {
    const auto playerIt = m_players.find(userId);
    if (playerIt == m_players.end()) {
        return;
    }

    command.text = Engine::Net::SanitizeChatText(std::move(command.text));
    if (!Engine::Net::IsValidChatText(command.text)) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const double nowSeconds =
        std::chrono::duration<double>(now.time_since_epoch()).count();
    constexpr double kMinChatIntervalSeconds = 0.25;
    const auto lastIt = m_lastChatSendSeconds.find(userId);
    if (lastIt != m_lastChatSendSeconds.end() &&
        nowSeconds - lastIt->second < kMinChatIntervalSeconds) {
        return;
    }
    m_lastChatSendSeconds[userId] = nowSeconds;

    Engine::Net::ChatMessage message;
    message.userId = userId;
    message.displayName = playerIt->second.displayName;
    message.text = command.text;
    message.tick = m_tickNumber;

    Engine::Net::GenericDataPayload payload;
    payload.userId = userId;
    payload.tick = message.tick;
    payload.channel = Engine::Net::kChatChannel;
    payload.data = Engine::Net::SerializeChatMessage(message).Data();
    m_server.BroadcastGenericData(payload);
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
