#include "ui/FontLibrary.h"
#include "../Logger.h"
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FONT_BAKING
#include "nuklear.h"

#include <fstream>
#include <vector>

using EngineLogging::Logger;

namespace Engine {
namespace Platform {
namespace UI {

const char* FontLibrary::RoleName(FontRole role) {
    switch (role) {
        case FontRole::UiRegular: return "UiRegular";
        case FontRole::UiBold: return "UiBold";
        case FontRole::UiSmall: return "UiSmall";
        case FontRole::Mono: return "Mono";
        case FontRole::Hud: return "Hud";
        case FontRole::Chat: return "Chat";
        default: return "Unknown";
    }
}

std::string FontLibrary::ResolvePath(const char* fileName) const {
    if (!fileName || fileName[0] == '\0') {
        return {};
    }
    if (m_config.fontsRoot.empty()) {
        return fileName;
    }
    return m_config.fontsRoot + "/" + fileName;
}

void FontLibrary::InitSlot(FontRole role, const char* fileName, float size) {
    const int index = static_cast<int>(role);
    Slot& slot = m_slots[index];
    slot.role = role;
    slot.fileName = fileName;
    slot.size = size;
    slot.resolvedPath = ResolvePath(fileName);
    slot.font = nullptr;
}

bool IsFontFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    unsigned char magic[4] = {};
    file.read(reinterpret_cast<char*>(magic), 4);
    if (file.gcount() < 4) {
        return false;
    }
    const bool trueType = magic[0] == 0x00 && magic[1] == 0x01 && magic[2] == 0x00 && magic[3] == 0x00;
    const bool openType = magic[0] == 'O' && magic[1] == 'T' && magic[2] == 'T' && magic[3] == 'O';
    const bool collection = magic[0] == 't' && magic[1] == 't' && magic[2] == 'c' && magic[3] == 'f';
    const bool trueTag = magic[0] == 't' && magic[1] == 'r' && magic[2] == 'u' && magic[3] == 'e';
    return trueType || openType || collection || trueTag;
}

bool FontLibrary::TryLoadSlot(struct nk_font_atlas* atlas, Slot& slot, bool logMissing) {
    if (!atlas || !slot.fileName || slot.resolvedPath.empty()) {
        return false;
    }

    std::ifstream probe(slot.resolvedPath, std::ios::binary);
    if (!probe.good()) {
        if (logMissing) {
            LOG_WARNING_FMT("FontLibrary", "Font not found: %s", slot.resolvedPath.c_str());
        }
        return false;
    }
    probe.close();

    if (!IsFontFile(slot.resolvedPath)) {
        LOG_WARNING_FMT("FontLibrary", "Skipping %s because it is not a font file", slot.resolvedPath.c_str());
        return false;
    }

    struct nk_font_config cfg = nk_font_config(slot.size);
    cfg.oversample_h = 2;
    cfg.oversample_v = 2;
    cfg.pixel_snap = nk_false;
    cfg.range = nk_font_default_glyph_ranges();

    struct nk_font* font = nk_font_atlas_add_from_file(
        atlas, slot.resolvedPath.c_str(), slot.size, &cfg);
    if (!font) {
        LOG_WARNING_FMT("FontLibrary", "Failed to load font: %s", slot.resolvedPath.c_str());
        return false;
    }

    if (!atlas->default_font) {
        atlas->default_font = font;
    }

    slot.font = font;
    LOG_INFO_FMT("FontLibrary", "Loaded %s from %s (%.0fpx)",
                 RoleName(slot.role), slot.resolvedPath.c_str(), slot.size);
    return true;
}

bool FontLibrary::LoadRole(struct nk_font_atlas* atlas, FontRole role, const char* fileName, float size,
                           const std::vector<std::string>& fallbacks) {
    InitSlot(role, fileName, size);
    Slot& slot = m_slots[static_cast<int>(role)];

    std::vector<std::string> candidates;
    if (!slot.resolvedPath.empty()) {
        candidates.push_back(slot.resolvedPath);
    }
    candidates.insert(candidates.end(), fallbacks.begin(), fallbacks.end());

    for (size_t i = 0; i < candidates.size(); ++i) {
        slot.resolvedPath = candidates[i];
        const bool logMissing = i + 1 == candidates.size();
        if (TryLoadSlot(atlas, slot, logMissing)) {
            return true;
        }
    }
    slot.font = nullptr;
    return false;
}

bool FontLibrary::BakeIntoAtlas(struct nk_font_atlas* atlas) {
    if (!atlas) {
        return false;
    }

#if defined(_WIN32)
    const std::vector<std::string> regularFallbacks = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    const std::vector<std::string> boldFallbacks = {
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/calibrib.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
    };
#else
    const std::vector<std::string> regularFallbacks;
    const std::vector<std::string> boldFallbacks;
#endif

    bool anyLoaded = false;
    anyLoaded = LoadRole(atlas, FontRole::UiRegular, "Inter-Regular.ttf", m_config.uiSize, regularFallbacks) ||
                anyLoaded;
    anyLoaded = LoadRole(atlas, FontRole::UiBold, "Inter-Bold.ttf", m_config.uiSize, boldFallbacks) || anyLoaded;
    anyLoaded = LoadRole(atlas, FontRole::Hud, "Inter-Bold.ttf", m_config.hudSize, boldFallbacks) || anyLoaded;

    if (!GetFont(FontRole::UiBold)) {
        m_slots[static_cast<int>(FontRole::UiBold)].font = GetFont(FontRole::UiRegular);
    }
    if (!GetFont(FontRole::UiRegular)) {
        m_slots[static_cast<int>(FontRole::UiRegular)].font = GetFont(FontRole::UiBold);
    }
    if (!GetFont(FontRole::Hud)) {
        m_slots[static_cast<int>(FontRole::Hud)].font = GetFont(FontRole::UiBold);
    }
    return anyLoaded;
}

bool FontLibrary::LoadChatFont(struct nk_font_atlas* atlas) {
    if (!atlas) {
        return false;
    }

    InitSlot(FontRole::Chat, "arial.ttf", m_config.chatSize);
    Slot& slot = m_slots[static_cast<int>(FontRole::Chat)];

    std::vector<std::string> candidates;
    if (!m_config.fontsRoot.empty()) {
        candidates.push_back(ResolvePath("arial.ttf"));
    }
#if defined(_WIN32)
    candidates.emplace_back("C:/Windows/Fonts/arial.ttf");
    candidates.emplace_back("C:/Windows/Fonts/Arial.ttf");
#elif defined(__APPLE__)
    candidates.emplace_back("/Library/Fonts/Arial.ttf");
    candidates.emplace_back("/System/Library/Fonts/Supplemental/Arial.ttf");
#else
    candidates.emplace_back("/usr/share/fonts/truetype/msttcorefonts/Arial.ttf");
    candidates.emplace_back("/usr/share/fonts/truetype/msttcorefonts/arial.ttf");
    candidates.emplace_back("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf");
#endif

    for (size_t i = 0; i < candidates.size(); ++i) {
        slot.resolvedPath = candidates[i];
        const bool logMissing = i + 1 == candidates.size();
        if (TryLoadSlot(atlas, slot, logMissing)) {
            return true;
        }
    }
    return false;
}

void FontLibrary::ApplyDefaultFont(struct nk_context* ctx) const {
    if (!ctx) {
        return;
    }
    struct nk_font* regular = GetFont(FontRole::UiRegular);
    if (regular) {
        nk_style_set_font(ctx, &regular->handle);
    }
}

struct nk_font* FontLibrary::GetFont(FontRole role) const {
    const int index = static_cast<int>(role);
    if (index < 0 || index >= static_cast<int>(FontRole::Count)) {
        return nullptr;
    }
    return m_slots[index].font;
}

const char* FontLibrary::GetPath(FontRole role) const {
    const int index = static_cast<int>(role);
    if (index < 0 || index >= static_cast<int>(FontRole::Count)) {
        return nullptr;
    }
    const std::string& path = m_slots[index].resolvedPath;
    return path.empty() ? nullptr : path.c_str();
}

bool FontLibrary::IsLoaded(FontRole role) const {
    return GetFont(role) != nullptr;
}

bool FontLibrary::HasAnyCustomFont() const {
    for (const auto& slot : m_slots) {
        if (slot.font) {
            return true;
        }
    }
    return false;
}

void FontLibrary::SetBakedFont(FontRole role, struct nk_font* font) {
    const int index = static_cast<int>(role);
    if (index < 0 || index >= static_cast<int>(FontRole::Count)) {
        return;
    }
    m_slots[index].font = font;
}

} // namespace UI
} // namespace Platform
} // namespace Engine
