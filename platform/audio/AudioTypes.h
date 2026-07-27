#pragma once

#include <glm/glm.hpp>
#include <string>

namespace Engine {
namespace Audio {

enum class SoundState {
    Stopped,
    Playing,
    Paused,
};

struct SoundHandle {
    uint32_t id = 0;
    bool IsValid() const { return id != 0; }
};

} // namespace Audio
} // namespace Engine
