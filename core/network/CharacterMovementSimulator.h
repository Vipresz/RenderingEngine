#pragma once

#include "core/network/CharacterMovementState.h"
#include "core/network/MovementCommand.h"

#include <functional>

namespace Engine {
namespace Net {

using TerrainHeightSampler = std::function<float(float x, float z)>;

class CharacterMovementSimulator {
public:
    static void SimulateStep(CharacterMovementState& state,
                             const MovementCommand& command,
                             const TerrainHeightSampler& terrainSampler);

    static float LerpAngleDegrees(float from, float to, float t);
};

} // namespace Net
} // namespace Engine
