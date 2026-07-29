#include "network/ItemOwnershipReplicationBinding.h"

#include "core/ecs/components/PlayerItemOwnershipComponent.h"
#include "core/network/ItemOwnershipReplication.h"

namespace Engine {
namespace Platform {
namespace Networking {

void RegisterItemOwnershipReplication(Systems::ReplicationSystem& replication) {
    Engine::Net::ReplicationComponentDescriptor descriptor;
    descriptor.id = Engine::Net::kItemOwnershipReplicationComponent;
    descriptor.name = "ItemOwnership";
    replication.GetRegistry().Register(std::move(descriptor));

    replication.RegisterComponentBinding(Systems::ReplicationComponentBinding{
        Engine::Net::kItemOwnershipReplicationComponent,
        [](EntityID entity, ECS::Registry& registry, Engine::Net::NetBuffer& out) {
            const ECS::PlayerItemOwnershipComponent* ownership =
                registry.GetComponent<ECS::PlayerItemOwnershipComponent>(entity);
            if (!ownership || ownership->ownerUserId == Engine::Net::kInvalidUserId) {
                return false;
            }
            out = Engine::Net::FromOwnershipComponent(*ownership).Serialize();
            return true;
        },
        [](EntityID entity, ECS::Registry& registry, const Engine::Net::NetBuffer& in) {
            Engine::Net::ReplicatedItemOwnership replicated;
            if (!Engine::Net::ReplicatedItemOwnership::Deserialize(in, replicated)) {
                return false;
            }
            ECS::PlayerItemOwnershipComponent* ownership =
                registry.GetComponent<ECS::PlayerItemOwnershipComponent>(entity);
            if (!ownership) {
                ownership = &registry.AddComponent<ECS::PlayerItemOwnershipComponent>(entity);
            }
            *ownership = Engine::Net::ToOwnershipComponent(replicated);
            return true;
        }});
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
