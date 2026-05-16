#pragma once

#include <string>

namespace Engine {
namespace Platform {
namespace Render {

/// From a concrete mesh path or an asset folder (FBX/OBJ next to tex/), return the preferred mesh file.
std::string ResolvePrimaryMeshFile(const std::string& pathOrDir);

/// Same as ResolvePrimaryMeshFile, but falls back to the input when it is an existing regular file with an
/// unknown extension (Assimp may still load it).
std::string ResolveMeshLoadPath(const std::string& pathOrDir);

} // namespace Render
} // namespace Platform
} // namespace Engine
