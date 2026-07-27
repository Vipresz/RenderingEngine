#pragma once

#include <string>

struct nk_context;
struct nk_font;
struct nk_font_atlas;

namespace Engine {
namespace Platform {
namespace UI {

// Built-in font roles for menus and in-game HUD text.
enum class FontRole {
    UiRegular,
    UiBold,
    UiSmall,
    Mono,
    Hud,
    Count
};

// Loads TTF files into Nuklear's font atlas and exposes paths for future renderers.
class FontLibrary {
public:
    struct Config {
        std::string fontsRoot = "menu/fonts";
        float uiSize = 18.0f;
        float uiSmallSize = 14.0f;
        float monoSize = 16.0f;
        float hudSize = 20.0f;
    };

    void SetConfig(const Config& config) { m_config = config; }
    const Config& GetConfig() const { return m_config; }

    // Call between nk_glfw3_font_stash_begin/end. Returns false if no custom fonts loaded.
    bool BakeIntoAtlas(struct nk_font_atlas* atlas);

    void ApplyDefaultFont(struct nk_context* ctx) const;

    struct nk_font* GetFont(FontRole role) const;
    const char* GetPath(FontRole role) const;

    bool IsLoaded(FontRole role) const;
    bool HasAnyCustomFont() const;

    // Called after atlas bake to register fonts loaded outside FontLibrary.
    void SetBakedFont(FontRole role, struct nk_font* font);

    static const char* RoleName(FontRole role);

private:
    struct Slot {
        FontRole role = FontRole::UiRegular;
        const char* fileName = nullptr;
        float size = 18.0f;
        std::string resolvedPath;
        struct nk_font* font = nullptr;
    };

    void InitSlot(FontRole role, const char* fileName, float size);
    bool TryLoadSlot(struct nk_font_atlas* atlas, Slot& slot);
    std::string ResolvePath(const char* fileName) const;

    Config m_config;
    Slot m_slots[static_cast<int>(FontRole::Count)]{};
};

} // namespace UI
} // namespace Platform
} // namespace Engine
