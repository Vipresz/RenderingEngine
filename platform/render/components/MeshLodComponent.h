#pragma once

#include <cstdint>
#include <vector>

namespace Engine {
namespace Platform {
namespace Render {
    class Model;
}
}
}

namespace Engine {
namespace Platform {
namespace Render {
namespace Components {

struct MeshLodLevel {
    Model* model = nullptr;
    float maxViewDistance = 0.0f;
};

// Optional runtime LOD chain. Levels must be sorted by ascending maxViewDistance.
struct MeshLodComponent {
    std::vector<MeshLodLevel> levels;
};

} // namespace Components
} // namespace Render
} // namespace Platform
} // namespace Engine
