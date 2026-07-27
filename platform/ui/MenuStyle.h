#pragma once

#include <string>

struct nk_context;

namespace Engine {
namespace Platform {
namespace UI {

// RGBA color helper (0–255 per channel)
struct MenuColor {
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;

    static MenuColor Rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
};

// Layout and sizing — tweak these to match your game's look
struct MenuMetrics {
    float windowPaddingX = 16.0f;
    float windowPaddingY = 16.0f;
    float itemSpacing = 10.0f;
    float buttonPaddingX = 20.0f;
    float buttonPaddingY = 12.0f;
    float buttonRounding = 6.0f;
    float buttonBorder = 1.0f;
    float windowRounding = 8.0f;
    float windowBorder = 1.0f;
    float titleHeight = 36.0f;
    float fontSize = 18.0f;       // UiRegular / UiBold size in pixels
    float monoFontSize = 16.0f;   // JetBrains Mono for debug / counters
    float hudFontSize = 20.0f;    // In-game HUD labels
    float menuButtonWidth = 220.0f;
    float menuButtonHeight = 44.0f;
};

// Palette — all UI colors in one place
struct MenuPalette {
    MenuColor windowBackground{30, 32, 40, 230};
    MenuColor windowBorder{60, 65, 80, 255};
    MenuColor titleText{220, 225, 235, 255};
    MenuColor bodyText{200, 205, 215, 255};
    MenuColor buttonNormal{50, 55, 70, 255};
    MenuColor buttonHover{70, 90, 130, 255};
    MenuColor buttonActive{90, 120, 180, 255};
    MenuColor buttonText{230, 235, 245, 255};
    MenuColor scrollbar{80, 85, 100, 255};
    MenuColor overlayDim{0, 0, 0, 140};  // Full-screen pause backdrop
};

// Complete menu theme — apply to nk_context once or after changes
class MenuStyle {
public:
    MenuMetrics metrics;
    MenuPalette palette;
    std::string fontsRoot = "menus/main_menu/fonts";

    void Apply(struct nk_context* ctx) const;

    static MenuStyle DefaultDark();
    static MenuStyle Astral();  // Game-specific preset
};

} // namespace UI
} // namespace Platform
} // namespace Engine
