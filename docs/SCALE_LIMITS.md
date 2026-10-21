# Scale limits

These are the limits enforced by this engine's shard policy and replication budgets. AstralWorlds uses the same numbers for a zone.

## Supported today

| Target | Status |
|--------|--------|
| 100–200 players per zone instance layer | Supported with replication layering, interest caps, and UDP batching |
| 256 players per shard process | Supported (`shardHardCap`, overflow shard redirect) |
| Cross-shard login routing | Supported via the gateway and `ShardRegistry` |
| Mid-session zone handoff | Supported via `ZoneTransfer` and a signed `migrationToken` |

## Not supported

| Target | Reason |
|--------|--------|
| 1000 or more players on one shard process | The hard cap and the tick and replication budgets stop a single process there. Split the population with overflow shards and gateway routing. |
| Seamless in-world cross-shard migration without a reconnect | The client reconnects to the target shard. The migration token keeps the user id and inventory. |
| Global matchmaking or a dungeon finder | The gateway only redirects a login to the least-loaded shard. |

`config/join_load_stress_scale.json` may raise caps while profiling. A production zone should stay at 256 players per shard.
