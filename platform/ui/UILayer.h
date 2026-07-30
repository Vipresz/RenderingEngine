#pragma once

#include <cstdint>
#include <string_view>

namespace Engine {
namespace Platform {
namespace UI {

// Draw priority for Nuklear UI. Lower values are drawn first (behind); higher values on top.
//
// Typical in-game stacking (bottom → top):
//   WorldLabels → Hud → Prompts → Panels → Popups → ContextMenu
enum class UILayerId : int16_t {
    Background = 0,
    WorldLabels = 100,  // in-world nameplates, speech bubbles
    Hud = 200,          // minimap, character HUD, chat, bottom action bar
    Menus = 300,        // legacy / generic menu bucket
    Prompts = 350,      // top-center accept/decline prompts
    Panels = 400,       // opened overlay panels (paperdoll, bag, trade, settings)
    Popups = 500,       // contextual item menus (trade quantity, bag actions)
    ContextMenu = 600,
    Modal = 700,
    Debug = 900,
};

constexpr int16_t UILayerPriority(UILayerId layer) {
    return static_cast<int16_t>(layer);
}

constexpr std::string_view UILayerName(UILayerId layer) {
    switch (layer) {
        case UILayerId::Background:
            return "Background";
        case UILayerId::WorldLabels:
            return "WorldLabels";
        case UILayerId::Hud:
            return "Hud";
        case UILayerId::Menus:
            return "Menus";
        case UILayerId::Prompts:
            return "Prompts";
        case UILayerId::Panels:
            return "Panels";
        case UILayerId::Popups:
            return "Popups";
        case UILayerId::ContextMenu:
            return "ContextMenu";
        case UILayerId::Modal:
            return "Modal";
        case UILayerId::Debug:
            return "Debug";
        default:
            return "Unknown";
    }
}

} // namespace UI
} // namespace Platform
} // namespace Engine
