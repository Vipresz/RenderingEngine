#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

#include <string>
#include <vector>

struct nk_context;
struct nk_command_buffer;
struct nk_rect;
struct nk_user_font;

namespace Engine {
namespace Platform {
namespace UI {

struct ItemTooltipContent {
    std::string name;
    std::string typeLabel;
    std::string description;
};

struct ItemTooltipStyle {
    float padding = 10.0f;
    float maxWidth = 240.0f;
    float lineGap = 3.0f;
    float nameTypeGap = 4.0f;
    float typeDescriptionGap = 6.0f;
    float screenMargin = 8.0f;
    float cornerRadius = 6.0f;
    unsigned char backgroundAlpha = 220;
    unsigned char borderAlpha = 200;
};

// Measure and draw a multi-line item inspect panel (name, type, description).
struct nk_rect MeasureItemTooltip(const ItemTooltipContent& content,
                                  const struct nk_user_font* font,
                                  const ItemTooltipStyle& style = {});

void DrawItemTooltip(struct nk_command_buffer* canvas,
                     const struct nk_rect& bounds,
                     const ItemTooltipContent& content,
                     const struct nk_user_font* font,
                     const ItemTooltipStyle& style = {});

// Position near anchor (prefer above), clamp to screen, then draw.
void PresentItemTooltip(struct nk_context* ctx,
                          struct nk_command_buffer* canvas,
                          const struct nk_rect& anchor,
                          int screenWidth,
                          int screenHeight,
                          const ItemTooltipContent& content,
                          const ItemTooltipStyle& style = {});

} // namespace UI
} // namespace Platform
} // namespace Engine

#endif // PLATFORM_HAS_NUKLEAR
