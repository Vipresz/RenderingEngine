# Scale limits (AstralWorlds)

## Supported today

| Target | Status |
|--------|--------|
| 100–200 players / zone instance layer | Supported with replication layering, interest caps, UDP batching |
| 256 players / shard process | Supported (`shardHardCap`, overflow shard redirect) |
| Cross-shard login routing | Supported via `gateway` + `ShardRegistry` |
| Mid-session zone handoff | Supported via `ZoneTransfer` + signed `migrationToken` |

## Not supported

| Target | Reason |
|--------|--------|
| 1000+ players on a **single** shard process | Hard cap + tick/replication budgets; use overflow shards / gateway routing |
| Seamless in-world cross-shard migration without reconnect | Client reconnects to target shard (token preserves user id + inventory) |
| Global matchmaking / dungeon finder | Gateway only performs least-loaded shard redirect |

Stress configs (`join_load_stress_scale.json`) may raise caps for profiling; production zones should stay at 256/shard.
