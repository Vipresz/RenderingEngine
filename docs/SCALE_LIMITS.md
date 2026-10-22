# Scale limits

Numbers below are the defaults in `config/zone_default.json` and `core/network/ScalePolicyConfig.h`. A zone file can override them. This file does not describe another repository.

## What the default zone allows

| Limit | Where it comes from |
|--------|---------------------|
| 256 players on one shard | `shard.maxPlayers` and `scalePolicy.shardHardCap` in `config/zone_default.json`. `ShardRegistry` uses 256 when an endpoint omits `maxPlayers`. |
| Overflow when that shard is full | `overflowShardId` plus login redirect. The client connects to the redirect host. |
| Nearby interest, not a full-zone fanout | `layerFullRadius` 20, `layerMidRadius` 50, `layerFarRadius` 120. Observer caps are 64, or 20 when the cell is crowded. |
| Login routed to the least-loaded live shard | `NetGateway` calls `ShardRegistry::SelectShardForLogin`. |
| Mid-session move to another shard | `ZoneTransfer` and a signed `migrationToken`. The token stores the user id, source shard, target shard, and expiry. It does not store inventory. |

## What this engine does not do

| Claim | What the code does instead |
|--------|----------------------------|
| 100–200 players per layer as a fixed cap | No such constant. The layer code routes logins on the shard cap above. |
| 1000 players refused by the engine | Not a hard refusal. `config/join_load_stress_scale.json` sets `shardHardCap` to 1200, and `scripts/run_join_load_stress_profile.ps1` can run 1048 players. Keep a production zone at the 256-player file. |
| Seamless cross-shard move with no reconnect | The client reconnects. `FollowRedirect` opens a new TCP session and sends the migration token. |
| Matchmaking or a dungeon finder | The gateway only picks the registered shard with the lowest population that is still under its cap. |
