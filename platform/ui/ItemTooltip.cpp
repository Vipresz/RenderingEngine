#include "ui/ItemTooltip.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#include "nuklear.h"

#include <algorithm>
#include <cmath>
#include <cctype>

namespace Engine {
namespace Platform {
namespace UI {

namespace {

std::vector<std::string> WrapTextLines(const std::string& text,
                                       const struct nk_user_font* font,
                                       float maxTextWidth) {
    std::vector<std::string> lines;
    if (!font || text.empty() || maxTextWidth <= 1.0f) {
        if (!text.empty()) {
            lines.push_back(text);
        }
        return lines;
    }

    std::string current;
    size_t cursor = 0;
    while (cursor < text.size()) {
        while (cursor < text.size() &&
               std::isspace(static_cast<unsigned char>(text[cursor]))) {
            ++cursor;
        }
        if (cursor >= text.size()) {
            break;
        }

        size_t wordStart = cursor;
        while (cursor < text.size() &&
               !std::isspace(static_cast<unsigned char>(text[cursor]))) {
            ++cursor;
        }
        const std::string word = text.substr(wordStart, cursor - wordStart);

        const std::string candidate = current.empty() ? word : current + " " + word;
        const float candidateWidth =
            font->width(font->userdata,
                        font->height,
                        candidate.c_str(),
                        static_cast<int>(candidate.size()));

        if (candidateWidth <= maxTextWidth) {
            current = candidate;
            continue;
        }

        if (!current.empty()) {
            lines.push_back(current);
            current.clear();
            const float wordWidth =
                font->width(font->userdata,
                            font->height,
                            word.c_str(),
                            static_cast<int>(word.size()));
            if (wordWidth <= maxTextWidth) {
                current = word;
            } else {
                lines.push_back(word);
            }
        } else {
            lines.push_back(word);
        }
    }

    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

float TextLineWidth(const struct nk_user_font* font, const std::string& line) {
    if (!font || line.empty()) {
        return 0.0f;
    }
    return font->width(font->userdata,
                       font->height,
                       line.c_str(),
                       static_cast<int>(line.size()));
}

struct TooltipLayout {
    std::vector<std::string> descriptionLines;
    float width = 0.0f;
    float height = 0.0f;
};

TooltipLayout BuildTooltipLayout(const ItemTooltipContent& content,
                                 const struct nk_user_font* font,
                                 const ItemTooltipStyle& style) {
    TooltipLayout layout;
    if (!font) {
        return layout;
    }

    const float maxTextWidth = std::max(80.0f, style.maxWidth - style.padding * 2.0f);
    const float lineHeight = font->height + style.lineGap;

    float contentWidth = 0.0f;
    float contentHeight = 0.0f;

    if (!content.name.empty()) {
        contentWidth = std::max(contentWidth, TextLineWidth(font, content.name));
        contentHeight += font->height;
    }

    if (!content.typeLabel.empty()) {
        if (contentHeight > 0.0f) {
            contentHeight += style.nameTypeGap;
        }
        contentWidth = std::max(contentWidth, TextLineWidth(font, content.typeLabel));
        contentHeight += font->height;
    }

    if (!content.description.empty()) {
        layout.descriptionLines =
            WrapTextLines(content.description, font, maxTextWidth);
        if (!layout.descriptionLines.empty()) {
            if (contentHeight > 0.0f) {
                contentHeight += style.typeDescriptionGap;
            }
            for (const std::string& line : layout.descriptionLines) {
                contentWidth = std::max(contentWidth, TextLineWidth(font, line));
                contentHeight += lineHeight;
            }
            contentHeight -= style.lineGap;
        }
    }

    layout.width = std::min(style.maxWidth, contentWidth + style.padding * 2.0f);
    layout.height = contentHeight + style.padding * 2.0f;
    return layout;
}

struct nk_rect ClampTooltipPosition(struct nk_rect bounds,
                                    int screenWidth,
                                    int screenHeight,
                                    float margin) {
    if (bounds.x < margin) {
        bounds.x = margin;
    }
    if (bounds.y < margin) {
        bounds.y = margin;
    }
    if (bounds.x + bounds.w > static_cast<float>(screenWidth) - margin) {
        bounds.x = static_cast<float>(screenWidth) - margin - bounds.w;
    }
    if (bounds.y + bounds.h > static_cast<float>(screenHeight) - margin) {
        bounds.y = static_cast<float>(screenHeight) - margin - bounds.h;
    }
    return bounds;
}

} // namespace

struct nk_rect MeasureItemTooltip(const ItemTooltipContent& content,
                                  const struct nk_user_font* font,
                                  const ItemTooltipStyle& style) {
    const TooltipLayout layout = BuildTooltipLayout(content, font, style);
    return nk_rect(0.0f, 0.0f, layout.width, layout.height);
}

void DrawItemTooltip(struct nk_command_buffer* canvas,
                     const struct nk_rect& bounds,
                     const ItemTooltipContent& content,
                     const struct nk_user_font* font,
                     const ItemTooltipStyle& style) {
    if (!canvas || !font || bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const TooltipLayout layout = BuildTooltipLayout(content, font, style);
    if (layout.width <= 0.0f || layout.height <= 0.0f) {
        return;
    }

    const struct nk_color background = nk_rgba(16, 20, 32, style.backgroundAlpha);
    const struct nk_color border = nk_rgba(120, 140, 180, style.borderAlpha);
    const struct nk_color nameColor = nk_rgba(235, 240, 250, 255);
    const struct nk_color typeColor = nk_rgba(150, 175, 210, 255);
    const struct nk_color descriptionColor = nk_rgba(200, 210, 225, 255);
    const struct nk_color transparent = nk_rgba(0, 0, 0, 0);

    nk_fill_rect(canvas, bounds, style.cornerRadius, background);
    nk_stroke_rect(canvas, bounds, style.cornerRadius, 1.0f, border);

    float textY = bounds.y + style.padding;
    const float lineHeight = font->height + style.lineGap;

    if (!content.name.empty()) {
        nk_draw_text(canvas,
                     nk_rect(bounds.x + style.padding, textY, bounds.w, font->height),
                     content.name.c_str(),
                     static_cast<int>(content.name.size()),
                     font,
                     transparent,
                     nameColor);
        textY += font->height;
    }

    if (!content.typeLabel.empty()) {
        textY += style.nameTypeGap;
        nk_draw_text(canvas,
                     nk_rect(bounds.x + style.padding, textY, bounds.w, font->height),
                     content.typeLabel.c_str(),
                     static_cast<int>(content.typeLabel.size()),
                     font,
                     transparent,
                     typeColor);
        textY += font->height;
    }

    if (!layout.descriptionLines.empty()) {
        textY += style.typeDescriptionGap;
        for (const std::string& line : layout.descriptionLines) {
            nk_draw_text(canvas,
                         nk_rect(bounds.x + style.padding, textY, bounds.w, font->height),
                         line.c_str(),
                         static_cast<int>(line.size()),
                         font,
                         transparent,
                         descriptionColor);
            textY += lineHeight;
        }
    }
}

void PresentItemTooltip(struct nk_context* ctx,
                          struct nk_command_buffer* canvas,
                          const struct nk_rect& anchor,
                          int screenWidth,
                          int screenHeight,
                          const ItemTooltipContent& content,
                          const ItemTooltipStyle& style) {
    if (!ctx || !canvas || screenWidth <= 0 || screenHeight <= 0) {
        return;
    }
    if (content.name.empty()) {
        return;
    }

    const struct nk_user_font* font = ctx->style.font;
    if (!font) {
        return;
    }

    const TooltipLayout layout = BuildTooltipLayout(content, font, style);
    if (layout.width <= 0.0f || layout.height <= 0.0f) {
        return;
    }

    constexpr float kAnchorGap = 6.0f;
    float tooltipX = anchor.x + anchor.w * 0.5f - layout.width * 0.5f;
    float tooltipY = anchor.y - layout.height - kAnchorGap;

    if (tooltipY < style.screenMargin) {
        tooltipY = anchor.y + anchor.h + kAnchorGap;
    }

    struct nk_rect bounds = nk_rect(tooltipX, tooltipY, layout.width, layout.height);
    bounds = ClampTooltipPosition(bounds, screenWidth, screenHeight, style.screenMargin);
    DrawItemTooltip(canvas, bounds, content, font, style);
}

} // namespace UI
} // namespace Platform
} // namespace Engine
