# RenderingEngine

C++17 library for ECS, fixed-timestep simulation, OpenGL rendering, and authoritative multiplayer.

The window tries an OpenGL 4.3 context first, then 3.3. Ocean FFT shaders are `#version 430`. Mesh, shadow, and terrain shaders are `#version 330`.

## Layout

```
core/          ECS, timestep, jobs, scene, terrain, network payloads
platform/      OpenGL rendering, input, UI, NetServer/NetClient, physics, audio
config/        Zone and join-load scale JSON
docs/          Scale limits
examples/      basic_example and network_example
scripts/       Stress profiles and the coverage check
tests/         EngineTests
third_party/   Nuklear, miniaudio, and stb submodules
libs/          glm, glfw, glew, and assimp submodules
```

## Build

Fetch the submodules first. `libs/` holds glm, glfw, glew, and assimp. `third_party/` holds Nuklear 4.13.3, miniaudio 0.11.25, and stb (`stb_image` 2.16).

```powershell
git submodule update --init
```

From this directory, with Visual Studio 2022:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

A default configure builds `basic_example` and `network_example` and links them to the OpenGL platform library. Turning examples off also turns that library off unless `-DBUILD_ENGINE_PLATFORM=ON` is set.

## Tests

Core suite, no OpenGL:

```powershell
cmake -B build -DBUILD_ENGINE_EXAMPLES=OFF -DBUILD_ENGINE_TESTS=ON -DBUILD_ENGINE_PLATFORM=OFF
cmake --build build --config Release --target EngineTests
ctest --test-dir build -C Release --output-on-failure --label-exclude aggregate
```

Platform suite (OpenGL, Nuklear, TLS). Point `NUKLEAR_DIR` at `third_party/nuklear` when you want the UI cases:

```powershell
cmake -B build-platform -DBUILD_ENGINE_EXAMPLES=OFF -DBUILD_ENGINE_TESTS=ON -DBUILD_ENGINE_PLATFORM=ON -DENGINE_ENABLE_TLS=ON -DNUKLEAR_DIR="$pwd/third_party/nuklear"
cmake --build build-platform --config Release --target EngineTests
ctest --test-dir build-platform -C Release --output-on-failure -L platform --label-exclude aggregate
```

## Includes

```cpp
#include "core/ecs/Registry.h"
#include "core/System.h"
#include "platform/render/Window.h"
#include "platform/render/Shader.h"
```

`examples/BasicExample.cpp` opens a window, draws a ground plane and a sphere, and moves that sphere from the keyboard. `examples/NetworkExample.cpp` runs a `NetServer` or a `NetClient`.

## Scale

Shard caps and what the networking code does not implement are in [docs/SCALE_LIMITS.md](docs/SCALE_LIMITS.md).

`config/zone_default.json` is the 256-player zone. `config/join_load_scale.json` is the smaller join-load profile. `config/join_load_stress_scale.json` raises the cap for profiling. The stress runner is:

```powershell
.\scripts\run_join_load_stress_profile.ps1 -Build
```
