# Engine

Core game engine library providing ECS, rendering, input, and platform abstraction.

## Features

**Core Systems**
- Entity Component System (ECS) with component pools
- Job system for multithreaded task scheduling
- Fixed timestep simulation
- Deterministic math utilities

**Rendering** 
- OpenGL 3.3+ rendering pipeline
- Model loading (OBJ, FBX via Assimp)
- Skeletal animation system
- Materials, textures, shaders
- Grid renderer for debugging
- Multiple camera types (free, follow)

**Platform**
- Window management (GLFW)
- Input system (keyboard, mouse)
- Logger with file output
- Console redirect utility
- Nuklear UI integration

## Structure

```
engine/
├── core/              ECS, jobs, timestep, math
├── platform/          Rendering, input, logger, UI
└── examples/          Example applications
```

## Building

Built as part of the main project. See root README for build instructions.

**Standalone build:**
```powershell
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## Usage

```cpp
#include "core/ecs/Registry.h"
#include "core/System.h"
#include "platform/render/Window.h"
#include "platform/render/Shader.h"
```

See `examples/BasicExample.cpp` for a complete example.

## Gear 360

[Samsung Gear 360](https://github.com/Vipresz/SamsungGear360) is a live viewer for the camera's dual-fisheye stream. A Raspberry Pi relay brings the camera onto the local network, and the C++ and Python players can show the raw fisheye, a rectilinear view, or a draggable 360 view. Lens calibration can stitch the two lenses into one panorama.
