# RenderingEngine

C++17 library for ECS, fixed-timestep simulation, OpenGL rendering, and authoritative multiplayer.

The window requests an OpenGL 4.3 context so ocean FFT compute shaders can run, and falls back to 3.3. Game and editor shaders stay on 3.3.

## Layout

```
core/          ECS, timestep, jobs, scene, terrain, network payloads
platform/      OpenGL rendering, input, UI, NetServer/NetClient, physics, audio
config/        Zone and join-load scale JSON
docs/          Scale limits
examples/      basic_example and network_example
scripts/       Stress profiles and the coverage check
tests/         EngineTests
third_party/   Nuklear
```

## Build

From this directory, with Visual Studio 2022:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

`BUILD_ENGINE_EXAMPLES` defaults to ON and builds `basic_example` and `network_example`. `BUILD_ENGINE_PLATFORM` defaults to ON for a standalone configure, so the examples link the OpenGL platform library.

AstralWorlds consumes this tree as a subdirectory and turns the examples off. The commands above are the ones that configure this repository by itself.

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

`examples/BasicExample.cpp` opens a window, draws a grid, and reads keyboard input. `examples/NetworkExample.cpp` is the networking sample.

## Scale

Shard caps, overflow, and what is not supported are in [docs/SCALE_LIMITS.md](docs/SCALE_LIMITS.md).

Zone files live in `config/zone_default.json` and `config/join_load_stress_scale.json`. The stress runner is:

```powershell
.\scripts\run_join_load_stress_profile.ps1 -Build
```

## Related

[Samsung Gear 360](https://github.com/Vipresz/SamsungGear360) is a separate viewer for that camera's dual-fisheye stream. It is not part of this library. A Raspberry Pi relay puts the camera on the local network, and its C++ and Python players can show the raw fisheye, a rectilinear view, or a draggable 360 view. Lens calibration can stitch the two lenses into one panorama.
