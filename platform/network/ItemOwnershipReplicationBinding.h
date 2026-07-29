#pragma once

#include "network/systems/ReplicationSystem.h"

namespace Engine {
namespace Platform {
namespace Networking {

// Registers client-side ECS bindings for PlayerItemOwnershipComponent snapshots.
void RegisterItemOwnershipReplication(Systems::ReplicationSystem& replication);

} // namespace Networking
} // namespace Platform
} // namespace Engine
