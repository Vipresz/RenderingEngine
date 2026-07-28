#include "ui/FontLibrary.h"
#include "../Logger.h"
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FONT_BAKING
#include "nuklear.h"

#include <fstream>

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

bool FontLibrary::TryLoadSlot(struct nk_font_atlas* atlas, Slot& slot) {
    if (!atlas || !slot.fileName || slot.resolvedPath.empty()) {
        return false;
    }

    std::ifstream probe(slot.resolvedPath, std::ios::binary);
    if (!probe.good()) {
        LOG_WARNING_FMT("FontLibrary", "Font not found: %s", slot.resolvedPath.c_str());
        return false;
    }
    probe.close();

    struct nk_font_config cfg = nk_font_config(slot.size);
    cfg.oversample_h = 1;
    cfg.oversample_v = 1;
    cfg.pixel_snap = nk_true;
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

bool FontLibrary::BakeIntoAtlas(struct nk_font_atlas* atlas) {
    if (!atlas) {
        return false;
    }

    InitSlot(FontRole::Hud, "Inter-Bold.ttf", m_config.hudSize);

    bool anyLoaded = false;
    if (TryLoadSlot(atlas, m_slots[static_cast<int>(FontRole::Hud)])) {
        anyLoaded = true;
        struct nk_font* hud = GetFont(FontRole::Hud);
        m_slots[static_cast<int>(FontRole::UiRegular)].font = hud;
        m_slots[static_cast<int>(FontRole::UiBold)].font = hud;
    }
    return anyLoaded;
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
