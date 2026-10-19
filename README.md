# Engine

Core game engine library providing ECS, rendering, input, networking, and platform abstraction.

## Features

**Core Systems**
- Entity Component System (ECS) with component pools
- Job system for multithreaded task scheduling
- Fixed timestep simulation
- Deterministic math utilities

**Rendering**
- OpenGL 3.3+ rendering pipeline (shadow, opaque, transparent passes)
- Model loading (OBJ, FBX via Assimp)
- Skeletal animation system
- Materials, textures, shaders
- Grid renderer for debugging
- Multiple camera types (free, follow)

**Networking (authoritative multiplayer)**
- TCP + UDP (`NetServer`, `NetClient`, `UdpChannel`)
- `AuthoritativeGameSimulator` — movement, inventory, trade, chat
- Generic data channels: `movement`, `loadout`, `item_ownership`, `inventory_action`, `trade`, `chat`
- Replication layering + interest management (`InterestManager`, `ReplicationTier`)
- Zone scale policy + shard overflow (`LayerInstancePolicy`, `ZoneScaleConfig`)
- Client prediction + rollback (`NetworkSystem`, `InputBuffer`)
- Optional TLS (`ENGINE_ENABLE_TLS`) and credential store

**Platform**
- Window management (GLFW)
- Input system (keyboard, mouse)
- Nuklear UI (`NuklearUI`, `ItemTooltip`, `ItemActionPanel`, `Minimap`)
- Logger with file output
- Simple physics world + null/miniaudio audio backends

## Structure

```
RenderingEngine/
├── core/              ECS, jobs, timestep, math, network payloads
├── platform/          Rendering, input, logger, UI, NetServer/NetClient
├── config/            Zone and join-load scale JSON
├── scripts/           Stress profiles and coverage checks
└── tests/             EngineTests (platform tests need BUILD_ENGINE_PLATFORM=ON)
```

## Building

Built as part of the main AstralWorlds project. See root README for build instructions.

**Standalone build:**
```powershell
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**Platform + network tests:**
```powershell
cmake -B build-platform -DBUILD_ENGINE_TESTS=ON -DBUILD_ENGINE_PLATFORM=ON
cmake --build build-platform --config Release --target EngineTests
```

## Usage

```cpp
#include "core/ecs/Registry.h"
#include "core/System.h"
#include "platform/render/Window.h"
#include "platform/render/Shader.h"
```

See `examples/BasicExample.cpp` for a complete example.

## Scale and stress testing

Config: `config/join_load_stress_scale.json`, `config/zone_default.json`.

```powershell
.\scripts\run_join_load_stress_profile.ps1 -Build
```

See AstralWorlds skill `.cursor/skills/astral-network-scale/` for architecture, channel authority, and baseline metrics.

## Gear 360

[Samsung Gear 360](https://github.com/Vipresz/SamsungGear360) is a live viewer for the camera's dual-fisheye stream. A Raspberry Pi relay brings the camera onto the local network, and the C++ and Python players can show the raw fisheye, a rectilinear view, or a draggable 360 view. Lens calibration can stitch the two lenses into one panorama.
