#include "render/TerrainMaterial.h"

#include "Logger.h"
#include "render/Texture.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace Engine {
namespace Platform {
namespace Render {

namespace {

namespace fs = std::filesystem;

struct BaseMaps {
    std::string id;
    std::unique_ptr<Texture> albedo;
    std::unique_ptr<Texture> normalMap;
    std::unique_ptr<Texture> orm;
};

struct RegionPreset {
    std::string id;
    std::string layers[4];
    float tint[3] = {1.0f, 1.0f, 1.0f};
    float roughnessBias = 0.0f;
    float uvScale = 0.16f;
    float slopeRock = 0.45f;
    float heightCover = 0.0f;
    float coverAmount = 0.75f;
    float accentAmount = 0.3f;
    float moss = 0.0f;
    float snow = 0.0f;
    float ash = 0.0f;
    float dust = 0.0f;
    float wet = 0.0f;
    float cracks = 0.0f;
    float exoticAmount = 0.0f;
    float exoticPulse = 1.0f;
    float exoticColor[3] = {0.4f, 0.9f, 0.55f};
};

size_t FindValue(const std::string& json, const char* key) {
    const std::string pattern = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) {
        return std::string::npos;
    }
    const size_t colon = json.find(':', keyPos + pattern.size());
    if (colon == std::string::npos) {
        return std::string::npos;
    }
    size_t value = colon + 1;
    while (value < json.size() && std::isspace(static_cast<unsigned char>(json[value]))) {
        ++value;
    }
    return value;
}

float ReadFloat(const std::string& json, const char* key, float fallback) {
    const size_t value = FindValue(json, key);
    if (value == std::string::npos) {
        return fallback;
    }
    try {
        return std::stof(json.substr(value));
    } catch (...) {
        return fallback;
    }
}

void ReadVec3(const std::string& json, const char* key, float out[3], const float fallback[3]) {
    out[0] = fallback[0];
    out[1] = fallback[1];
    out[2] = fallback[2];
    const size_t value = FindValue(json, key);
    if (value == std::string::npos || value >= json.size() || json[value] != '[') {
        return;
    }
    const char* cursor = json.c_str() + value + 1;
    char* end = nullptr;
    for (int channel = 0; channel < 3; ++channel) {
        out[channel] = std::strtof(cursor, &end);
        if (end == cursor) {
            out[0] = fallback[0];
            out[1] = fallback[1];
            out[2] = fallback[2];
            return;
        }
        cursor = end;
    }
}

std::string ReadString(const std::string& json, const char* key) {
    const size_t value = FindValue(json, key);
    if (value == std::string::npos || value >= json.size() || json[value] != '"') {
        return {};
    }
    const size_t end = json.find('"', value + 1);
    if (end == std::string::npos) {
        return {};
    }
    return json.substr(value + 1, end - value - 1);
}

std::vector<std::string> ReadStringArray(const std::string& json, const char* key) {
    std::vector<std::string> values;
    const size_t value = FindValue(json, key);
    if (value == std::string::npos || value >= json.size() || json[value] != '[') {
        return values;
    }
    const size_t end = json.find(']', value + 1);
    if (end == std::string::npos) {
        return values;
    }
    size_t cursor = value + 1;
    while (cursor < end) {
        const size_t open = json.find('"', cursor);
        if (open == std::string::npos || open >= end) {
            break;
        }
        const size_t close = json.find('"', open + 1);
        if (close == std::string::npos || close > end) {
            break;
        }
        values.push_back(json.substr(open + 1, close - open - 1));
        cursor = close + 1;
    }
    return values;
}

std::string ReadFile(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

RegionPreset ParseRegion(const std::string& json) {
    RegionPreset preset;
    preset.id = ReadString(json, "id");
    const std::vector<std::string> layers = ReadStringArray(json, "layers");
    for (int i = 0; i < 4 && i < static_cast<int>(layers.size()); ++i) {
        preset.layers[i] = layers[static_cast<size_t>(i)];
    }
    const float white[3] = {1.0f, 1.0f, 1.0f};
    const float exoticFallback[3] = {0.4f, 0.9f, 0.55f};
    ReadVec3(json, "tint", preset.tint, white);
    ReadVec3(json, "exotic_color", preset.exoticColor, exoticFallback);
    preset.roughnessBias = ReadFloat(json, "roughness_bias", preset.roughnessBias);
    preset.uvScale = ReadFloat(json, "uv_scale", preset.uvScale);
    preset.slopeRock = ReadFloat(json, "slope_rock", preset.slopeRock);
    preset.heightCover = ReadFloat(json, "height_cover", preset.heightCover);
    preset.coverAmount = ReadFloat(json, "cover_amount", preset.coverAmount);
    preset.accentAmount = ReadFloat(json, "accent_amount", preset.accentAmount);
    preset.moss = ReadFloat(json, "moss", preset.moss);
    preset.snow = ReadFloat(json, "snow", preset.snow);
    preset.ash = ReadFloat(json, "ash", preset.ash);
    preset.dust = ReadFloat(json, "dust", preset.dust);
    preset.wet = ReadFloat(json, "wet", preset.wet);
    preset.cracks = ReadFloat(json, "cracks", preset.cracks);
    preset.exoticAmount = ReadFloat(json, "exotic_amount", preset.exoticAmount);
    preset.exoticPulse = ReadFloat(json, "exotic_pulse", preset.exoticPulse);
    return preset;
}

std::unique_ptr<Texture> LoadMap(const fs::path& path) {
    const std::string pathString = path.string();
    auto texture = std::make_unique<Texture>(pathString.c_str());
    if (!texture->LoadTexture()) {
        LOG_WARNING_FMT("TerrainMaterial", "Missing map %s", pathString.c_str());
        return nullptr;
    }
    return texture;
}

} // namespace

struct TerrainMaterialLibrary::Cache {
    std::unique_ptr<Texture> fallbackAlbedo;
    std::unique_ptr<Texture> fallbackNormal;
    std::unique_ptr<Texture> fallbackOrm;
    std::unique_ptr<Texture> fallbackMask;
    std::unique_ptr<Texture> materialMask;
    std::unique_ptr<Texture> flowMask;
    std::unordered_map<std::string, BaseMaps> bases;
    std::unordered_map<std::string, RegionPreset> regions;
};

TerrainMaterialLibrary::TerrainMaterialLibrary() : m_cache(std::make_unique<Cache>()) {}

TerrainMaterialLibrary::~TerrainMaterialLibrary() = default;

std::string TerrainMaterialLibrary::FindDefaultRoot() {
    if (const char* env = std::getenv("ASTRAL_MATERIAL_LIBRARY")) {
        const fs::path root(env);
        if (fs::exists(root / "manifest.json")) {
            return root.string();
        }
    }

    fs::path dir = fs::current_path();
    for (int depth = 0; depth < 8; ++depth) {
        const fs::path nested = dir / "assets" / "library" / "manifest.json";
        if (fs::exists(nested)) {
            return (dir / "assets" / "library").string();
        }
        const fs::path direct = dir / "library" / "manifest.json";
        if (fs::exists(direct)) {
            return (dir / "library").string();
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir) {
            break;
        }
        dir = dir.parent_path();
    }
    return {};
}

bool TerrainMaterialLibrary::Load(const std::string& libraryRoot) {
    m_root = libraryRoot;
    m_regionIds.clear();
    m_cache->bases.clear();
    m_cache->regions.clear();

    const fs::path root(libraryRoot);
    if (!fs::exists(root / "manifest.json")) {
        LOG_WARNING_FMT("TerrainMaterial", "No manifest at %s", libraryRoot.c_str());
        return false;
    }

    m_cache->fallbackAlbedo = std::make_unique<Texture>();
    m_cache->fallbackNormal = std::make_unique<Texture>();
    m_cache->fallbackOrm = std::make_unique<Texture>();
    m_cache->fallbackAlbedo->LoadSolidColor(140, 140, 140);
    m_cache->fallbackNormal->LoadSolidColor(128, 128, 255);
    m_cache->fallbackOrm->LoadSolidColor(255, 210, 0);
    m_cache->fallbackMask = std::make_unique<Texture>();
    m_cache->fallbackMask->LoadSolidColor(0, 0, 0, 0);

    const std::string manifest = ReadFile(root / "manifest.json");
    m_regionIds = ReadStringArray(manifest, "regions");
    if (m_regionIds.empty()) {
        for (const fs::directory_entry& entry : fs::directory_iterator(root / "regions")) {
            if (entry.path().extension() == ".json") {
                m_regionIds.push_back(entry.path().stem().string());
            }
        }
    }

    for (const std::string& regionId : m_regionIds) {
        const fs::path path = root / "regions" / (regionId + ".json");
        const std::string json = ReadFile(path);
        if (json.empty()) {
            LOG_WARNING_FMT("TerrainMaterial", "Missing region %s", path.string().c_str());
            continue;
        }
        RegionPreset preset = ParseRegion(json);
        if (preset.id.empty()) {
            preset.id = regionId;
        }
        bool layersOk = true;
        for (const std::string& layer : preset.layers) {
            if (layer.empty()) {
                layersOk = false;
            }
        }
        if (!layersOk) {
            LOG_WARNING_FMT("TerrainMaterial", "Region %s needs 4 layers", regionId.c_str());
            continue;
        }
        m_cache->regions.emplace(preset.id, preset);

        for (const std::string& layer : preset.layers) {
            if (m_cache->bases.find(layer) != m_cache->bases.end()) {
                continue;
            }
            BaseMaps maps;
            maps.id = layer;
            const fs::path folder = root / "bases" / layer;
            maps.albedo = LoadMap(folder / "albedo.png");
            maps.normalMap = LoadMap(folder / "normal.png");
            maps.orm = LoadMap(folder / "orm.png");
            m_cache->bases.emplace(layer, std::move(maps));
        }
    }

    const fs::path terrainDir = root / "terrain";
    if (fs::exists(terrainDir / "material_mask.png")) {
        m_cache->materialMask = LoadMap(terrainDir / "material_mask.png");
    }
    if (fs::exists(terrainDir / "flow_mask.png")) {
        m_cache->flowMask = LoadMap(terrainDir / "flow_mask.png");
    }
    if (m_cache->materialMask) {
        LOG_INFO("TerrainMaterial", "Using Gaea material mask");
    }

    LOG_INFO_FMT("TerrainMaterial", "Loaded %zu regions from %s",
                 m_cache->regions.size(), libraryRoot.c_str());
    return !m_cache->regions.empty();
}

bool TerrainMaterialLibrary::Apply(Components::TerrainMaterialComponent& out, const std::string& regionId) const {
    const auto regionIt = m_cache->regions.find(regionId);
    if (regionIt == m_cache->regions.end()) {
        LOG_WARNING_FMT("TerrainMaterial", "Unknown region %s", regionId.c_str());
        return false;
    }
    const RegionPreset& preset = regionIt->second;

    auto textureId = [](const Texture* texture, const Texture* fallback) -> GLuint {
        if (texture && texture->IsLoaded()) {
            return texture->GetID();
        }
        return fallback && fallback->IsLoaded() ? fallback->GetID() : 0;
    };

    for (int i = 0; i < 4; ++i) {
        const auto baseIt = m_cache->bases.find(preset.layers[i]);
        if (baseIt == m_cache->bases.end()) {
            out.albedo[i] = textureId(nullptr, m_cache->fallbackAlbedo.get());
            out.normalMap[i] = textureId(nullptr, m_cache->fallbackNormal.get());
            out.orm[i] = textureId(nullptr, m_cache->fallbackOrm.get());
            continue;
        }
        const BaseMaps& maps = baseIt->second;
        out.albedo[i] = textureId(maps.albedo.get(), m_cache->fallbackAlbedo.get());
        out.normalMap[i] = textureId(maps.normalMap.get(), m_cache->fallbackNormal.get());
        out.orm[i] = textureId(maps.orm.get(), m_cache->fallbackOrm.get());
    }

    out.regionId = preset.id;
    out.tint[0] = preset.tint[0];
    out.tint[1] = preset.tint[1];
    out.tint[2] = preset.tint[2];
    out.roughnessBias = preset.roughnessBias;
    out.uvScale = preset.uvScale;
    out.slopeRock = preset.slopeRock;
    out.heightCover = preset.heightCover;
    out.coverAmount = preset.coverAmount;
    out.accentAmount = preset.accentAmount;
    out.moss = preset.moss;
    out.snow = preset.snow;
    out.ash = preset.ash;
    out.dust = preset.dust;
    out.wet = preset.wet;
    out.cracks = preset.cracks;
    out.exoticAmount = preset.exoticAmount;
    out.exoticPulse = preset.exoticPulse;
    out.exoticColor[0] = preset.exoticColor[0];
    out.exoticColor[1] = preset.exoticColor[1];
    out.exoticColor[2] = preset.exoticColor[2];
    out.materialMask = textureId(m_cache->materialMask.get(), m_cache->fallbackMask.get());
    out.flowMask = textureId(m_cache->flowMask.get(), m_cache->fallbackMask.get());
    out.useMasks = (m_cache->materialMask && m_cache->materialMask->IsLoaded()) ? 1.0f : 0.0f;
    out.active = true;
    return true;
}

} // namespace Render
} // namespace Platform
} // namespace Engine
