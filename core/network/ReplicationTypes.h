#pragma once

#include "core/Types.h"
#include "core/network/NetBuffer.h"
#include "core/network/NetTypes.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {
namespace Net {

using NetEntityId = uint32_t;
using ReplicationComponentId = uint16_t;

constexpr NetEntityId kInvalidNetEntity = 0;
constexpr ReplicationComponentId kInvalidReplicationComponent = 0;

// Per-component replicated state blob.
struct ComponentSnapshot {
    NetEntityId entityId = kInvalidNetEntity;
    ReplicationComponentId componentId = kInvalidReplicationComponent;
    uint64_t tick = 0;
    std::vector<uint8_t> data;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU32(entityId);
        buffer.WriteU16(componentId);
        buffer.WriteU64(tick);
        buffer.WriteBlob(data);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, ComponentSnapshot& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        return copy.ReadU32(out.entityId) &&
               copy.ReadU16(out.componentId) &&
               copy.ReadU64(out.tick) &&
               copy.ReadBlob(out.data);
    }
};

// Entity with multiple component snapshots.
struct EntitySnapshot {
    NetEntityId entityId = kInvalidNetEntity;
    uint64_t tick = 0;
    std::vector<ComponentSnapshot> components;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU32(entityId);
        buffer.WriteU64(tick);
        const uint16_t count = static_cast<uint16_t>(components.size());
        buffer.WriteU16(count);
        for (const ComponentSnapshot& component : components) {
            const NetBuffer componentBuffer = component.Serialize();
            buffer.WriteBlob(componentBuffer.Data());
        }
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, EntitySnapshot& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        uint16_t count = 0;
        if (!copy.ReadU32(out.entityId) || !copy.ReadU64(out.tick) || !copy.ReadU16(count)) {
            return false;
        }
        out.components.clear();
        out.components.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            std::vector<uint8_t> blob;
            if (!copy.ReadBlob(blob)) {
                return false;
            }
            ComponentSnapshot component;
            if (!ComponentSnapshot::Deserialize(NetBuffer(std::move(blob)), component)) {
                return false;
            }
            out.components.push_back(std::move(component));
        }
        return true;
    }
};

// Full world state for late join / resync.
struct WorldSnapshotPayload {
    uint64_t tick = 0;
    std::vector<EntitySnapshot> entities;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU64(tick);
        const uint32_t count = static_cast<uint32_t>(entities.size());
        buffer.WriteU32(count);
        for (const EntitySnapshot& entity : entities) {
            const NetBuffer entityBuffer = entity.Serialize();
            buffer.WriteBlob(entityBuffer.Data());
        }
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, WorldSnapshotPayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        uint32_t count = 0;
        if (!copy.ReadU64(out.tick) || !copy.ReadU32(count)) {
            return false;
        }
        out.entities.clear();
        out.entities.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            std::vector<uint8_t> blob;
            if (!copy.ReadBlob(blob)) {
                return false;
            }
            EntitySnapshot entity;
            if (!EntitySnapshot::Deserialize(NetBuffer(std::move(blob)), entity)) {
                return false;
            }
            out.entities.push_back(std::move(entity));
        }
        return true;
    }
};

using ComponentSerializeFn = std::function<bool(const void* component, NetBuffer& out)>;
using ComponentDeserializeFn = std::function<bool(const NetBuffer& in, void* component)>;

struct ReplicationComponentDescriptor {
    ReplicationComponentId id = kInvalidReplicationComponent;
    std::string name;
    ComponentSerializeFn serialize;
    ComponentDeserializeFn deserialize;
};

// Registry of component types that can be replicated over the network.
class ReplicationRegistry {
public:
    ReplicationComponentId Register(ReplicationComponentDescriptor descriptor) {
        if (descriptor.id == kInvalidReplicationComponent) {
            descriptor.id = m_nextId++;
        }
        m_byId[descriptor.id] = std::move(descriptor);
        return descriptor.id;
    }

    const ReplicationComponentDescriptor* Find(ReplicationComponentId id) const {
        const auto it = m_byId.find(id);
        return it == m_byId.end() ? nullptr : &it->second;
    }

    const std::unordered_map<ReplicationComponentId, ReplicationComponentDescriptor>& All() const {
        return m_byId;
    }

private:
    ReplicationComponentId m_nextId = 1;
    std::unordered_map<ReplicationComponentId, ReplicationComponentDescriptor> m_byId;
};

} // namespace Net
} // namespace Engine
