#include "network/TradeReplicationBinding.h"

#include "core/ecs/components/PlayerTradeComponent.h"
#include "core/network/TradeReplication.h"

namespace Engine {
namespace Platform {
namespace Networking {

void RegisterTradeReplication(Systems::ReplicationSystem& replication) {
    Engine::Net::ReplicationComponentDescriptor descriptor;
    descriptor.id = Engine::Net::kTradeReplicationComponent;
    descriptor.name = "Trade";
    replication.GetRegistry().Register(std::move(descriptor));

    replication.RegisterComponentBinding(Systems::ReplicationComponentBinding{
        Engine::Net::kTradeReplicationComponent,
        [](EntityID entity, ECS::Registry& registry, Engine::Net::NetBuffer& out) {
            const ECS::PlayerTradeComponent* trade = registry.GetComponent<ECS::PlayerTradeComponent>(entity);
            if (!trade || !trade->IsActive()) {
                return false;
            }
            out = Engine::Net::FromTradeView(*trade).Serialize();
            return true;
        },
        [](EntityID entity, ECS::Registry& registry, const Engine::Net::NetBuffer& in) {
            Engine::Net::ReplicatedPlayerTrade replicated;
            if (!Engine::Net::ReplicatedPlayerTrade::Deserialize(in, replicated)) {
                return false;
            }
            ECS::PlayerTradeComponent* trade = registry.GetComponent<ECS::PlayerTradeComponent>(entity);
            if (!trade) {
                trade = &registry.AddComponent<ECS::PlayerTradeComponent>(entity);
            }
            *trade = Engine::Net::ToTradeComponent(replicated);
            return true;
        }});
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
