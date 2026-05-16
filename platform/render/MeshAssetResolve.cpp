#include "render/MeshAssetResolve.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace Engine {
namespace Platform {
namespace Render {

namespace {

namespace fs = std::filesystem;

std::string StripTrailingSeparators(std::string s) {
    while (!s.empty() && (s.back() == '/' || s.back() == '\\')) {
        s.pop_back();
    }
    return s;
}

std::string PreferredPathString(fs::path p) {
    return p.make_preferred().string();
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool IsKnownMeshExtension(const std::string& extLower) {
    static const std::unordered_set<std::string> kExt{
        ".obj",  ".fbx",  ".dae",  ".3ds",  ".blend", ".gltf", ".glb",
        ".ply",  ".stl",  ".x",    ".ase",  ".md5mesh", ".mdl", ".smd"};
    return kExt.count(extLower) > 0;
}

bool IsObjExtension(const std::string& extLower) {
    return extLower == ".obj";
}

int LodPreferenceScore(const fs::path& meshPath) {
    std::string s = ToLower(meshPath.filename().string());
    int score = 0;
    if (s.find("100k") != std::string::npos) {
        score += 100;
    }
    if (s.find("200k") != std::string::npos) {
        score += 200;
    }
    if (s.find("30k") != std::string::npos) {
        score -= 40;
    }
    if (s.find("10k") != std::string::npos) {
        score -= 30;
    }
    if (s.find("high") != std::string::npos) {
        score += 40;
    }
    if (s.find("lod0") != std::string::npos) {
        score -= 15;
    }
    if (s.find("lod1") != std::string::npos) {
        score += 15;
    }
    return score;
}

int MeshFormatRank(const std::string& extLower) {
    if (IsObjExtension(extLower)) {
        return 0;
    }
    if (extLower == ".fbx") {
        return 1;
    }
    return 2;
}

bool PreferMeshCandidate(const fs::path& a, const fs::path& b) {
    const std::string ea = ToLower(a.extension().string());
    const std::string eb = ToLower(b.extension().string());
    const int ra = MeshFormatRank(ea);
    const int rb = MeshFormatRank(eb);
    if (ra != rb) {
        return ra < rb;
    }
    const int sa = LodPreferenceScore(a);
    const int sb = LodPreferenceScore(b);
    if (sa != sb) {
        return sa < sb;
    }
    return a.filename().string() < b.filename().string();
}

} // namespace

std::string ResolvePrimaryMeshFile(const std::string& pathOrDir) {
    if (pathOrDir.empty()) {
        return {};
    }

    const std::string cleaned = StripTrailingSeparators(pathOrDir);
    fs::path p(cleaned);
    std::error_code ec;
    const bool exists = fs::exists(p, ec);
    if (ec || !exists) {
        return {};
    }

    if (fs::is_regular_file(p, ec)) {
        const std::string ext = ToLower(p.extension().string());
        if (!IsKnownMeshExtension(ext)) {
            return {};
        }
        std::error_code canonEc;
        fs::path canon = weakly_canonical(p, canonEc);
        return PreferredPathString(canonEc ? fs::path(p) : std::move(canon));
    }

    if (!fs::is_directory(p, ec)) {
        return {};
    }

    std::vector<fs::path> meshes;
    try {
        for (const fs::directory_entry& ent : fs::directory_iterator(p)) {
            std::error_code regEc;
            if (!ent.is_regular_file(regEc) || regEc) {
                continue;
            }
            const fs::path f = ent.path();
            const std::string ext = ToLower(f.extension().string());
            if (IsKnownMeshExtension(ext)) {
                meshes.push_back(f);
            }
        }
    } catch (const fs::filesystem_error&) {
        return {};
    }
    if (meshes.empty()) {
        return {};
    }

    std::sort(meshes.begin(), meshes.end(), PreferMeshCandidate);
    std::error_code canonEc;
    fs::path chosen = weakly_canonical(meshes.front(), canonEc);
    return PreferredPathString(canonEc ? fs::path(meshes.front()) : std::move(chosen));
}

std::string ResolveMeshLoadPath(const std::string& pathOrDir) {
    const std::string primary = ResolvePrimaryMeshFile(pathOrDir);
    if (!primary.empty()) {
        return primary;
    }

    const std::string cleaned = StripTrailingSeparators(pathOrDir);
    fs::path p(cleaned);
    std::error_code ec;
    if (!fs::exists(p, ec) || ec) {
        return {};
    }

    if (fs::is_regular_file(p, ec)) {
        fs::path canon = weakly_canonical(p, ec);
        return PreferredPathString(ec ? fs::path(p) : std::move(canon));
    }
    return {};
}

} // namespace Render
} // namespace Platform
} // namespace Engine
