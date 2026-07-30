#include "ui/DecisionPrompt.h"

#ifdef PLATFORM_HAS_NUKLEAR

#include "ui/OverlayWindow.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "nuklear.h"

namespace Engine {
namespace Platform {
namespace UI {

namespace {

bool PointInRect(const struct nk_rect& bounds, float x, float y) {
    return x >= bounds.x && y >= bounds.y && x <= bounds.x + bounds.w && y <= bounds.y + bounds.h;
}

} // namespace

void DecisionPrompt::SetContent(DecisionPromptContent content) {
    m_content = std::move(content);
}

void DecisionPrompt::SetPrimaryCallback(std::function<void()> callback) {
    m_primaryCallback = std::move(callback);
}

void DecisionPrompt::SetSecondaryCallback(std::function<void()> callback) {
    m_secondaryCallback = std::move(callback);
}

bool DecisionPrompt::ComputeLayout(struct nk_context* ctx,
                                   int screenWidth,
                                   int screenHeight,
                                   struct nk_rect& outPanel,
                                   struct nk_rect& outPrimary,
                                   struct nk_rect& outSecondary,
                                   float& outRowH) const {
    if (!ctx || screenWidth <= 0 || screenHeight <= 0) {
        return false;
    }

    const struct nk_user_font* font = ctx->style.font;
    if (!font) {
        return false;
    }

    outRowH = static_cast<float>(font->height) + 12.0f;
    const float panelW = 360.0f;
    const float panelH = outRowH * 3.0f + 36.0f;
    const float panelX = static_cast<float>(screenWidth) * 0.5f - panelW * 0.5f;
    const float panelY = 16.0f;
    outPanel = nk_rect(panelX, panelY, panelW, panelH);

    const float buttonGap = 12.0f;
    const float buttonY = outPanel.y + panelH - outRowH - 14.0f;
    const bool showBoth = m_content.showPrimary && m_content.showSecondary;
    const float buttonW = showBoth ? (panelW - 30.0f - buttonGap) * 0.5f : panelW - 30.0f;

    if (m_content.showPrimary) {
        outPrimary = nk_rect(outPanel.x + 15.0f, buttonY, buttonW, outRowH);
    } else {
        outPrimary = nk_rect(0.0f, 0.0f, 0.0f, 0.0f);
    }

    if (m_content.showSecondary) {
        const float secondaryX = showBoth ? outPrimary.x + outPrimary.w + buttonGap : outPanel.x + 15.0f;
        outSecondary = nk_rect(secondaryX, buttonY, buttonW, outRowH);
    } else {
        outSecondary = nk_rect(0.0f, 0.0f, 0.0f, 0.0f);
    }

    return true;
}

void DecisionPrompt::Draw(struct nk_context* ctx, int screenWidth, int screenHeight) {
    if (!ctx || !m_visible) {
        return;
    }

    const struct nk_user_font* font = ctx->style.font;
    if (!font) {
        return;
    }

    struct nk_rect panelBounds{};
    struct nk_rect primaryBounds{};
    struct nk_rect secondaryBounds{};
    float rowH = 0.0f;
    if (!ComputeLayout(ctx, screenWidth, screenHeight, panelBounds, primaryBounds, secondaryBounds, rowH)) {
        return;
    }

    OverlayScope overlay(ctx, "DecisionPrompt", OverlayScope::FullScreen(screenWidth, screenHeight));
    if (!overlay.IsOpen() || !overlay.Canvas()) {
        return;
    }

    struct nk_command_buffer* canvas = overlay.Canvas();
    const struct nk_color background = nk_rgba(16, 20, 32, 236);
    const struct nk_color border = nk_rgba(120, 140, 180, 210);
    const struct nk_color titleColor = nk_rgba(235, 240, 250, 255);
    const struct nk_color messageColor = nk_rgba(180, 195, 220, 255);
    const struct nk_color buttonFill = nk_rgba(34, 42, 58, 230);
    const struct nk_color buttonHoverFill = nk_rgba(52, 66, 92, 240);
    const struct nk_color buttonText = nk_rgba(225, 232, 245, 255);
    const struct nk_color transparent = nk_rgba(0, 0, 0, 0);

    nk_fill_rect(canvas, panelBounds, 8.0f, background);
    nk_stroke_rect(canvas, panelBounds, 8.0f, 1.0f, border);

    const std::string& title = m_content.title;
    const int titleLen = static_cast<int>(std::min(title.size(), size_t{48}));
    nk_draw_text(canvas,
                 nk_rect(panelBounds.x + 14.0f, panelBounds.y + 10.0f, panelBounds.w - 28.0f, rowH),
                 title.c_str(),
                 titleLen,
                 font,
                 transparent,
                 titleColor);

    const std::string& message = m_content.message;
    const int messageLen = static_cast<int>(std::min(message.size(), size_t{96}));
    nk_draw_text(canvas,
                 nk_rect(panelBounds.x + 14.0f, panelBounds.y + 10.0f + rowH, panelBounds.w - 28.0f, rowH),
                 message.c_str(),
                 messageLen,
                 font,
                 transparent,
                 messageColor);

    const auto drawButton = [&](const struct nk_rect& bounds, const std::string& label, bool hover) {
        if (bounds.w <= 0.0f || bounds.h <= 0.0f || label.empty()) {
            return;
        }
        nk_fill_rect(canvas, bounds, 6.0f, hover ? buttonHoverFill : buttonFill);
        nk_stroke_rect(canvas, bounds, 6.0f, 1.0f, border);
        const int labelLen = static_cast<int>(std::min(label.size(), size_t{24}));
        const float textW = font->width(font->userdata, font->height, label.c_str(), labelLen);
        nk_draw_text(canvas,
                     nk_rect(bounds.x + (bounds.w - textW) * 0.5f,
                             bounds.y + (bounds.h - font->height) * 0.5f,
                             textW,
                             font->height),
                     label.c_str(),
                     labelLen,
                     font,
                     transparent,
                     buttonText);
    };

    const bool hoverPrimary =
        m_content.showPrimary && nk_input_is_mouse_hovering_rect(&ctx->input, primaryBounds);
    const bool hoverSecondary =
        m_content.showSecondary && nk_input_is_mouse_hovering_rect(&ctx->input, secondaryBounds);
    drawButton(primaryBounds, m_content.primaryLabel, hoverPrimary);
    drawButton(secondaryBounds, m_content.secondaryLabel, hoverSecondary);

    if (nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT)) {
        const float clickX = ctx->input.mouse.pos.x;
        const float clickY = ctx->input.mouse.pos.y;
        if (m_content.showPrimary && PointInRect(primaryBounds, clickX, clickY) && m_primaryCallback) {
            m_primaryCallback();
            return;
        }
        if (m_content.showSecondary && PointInRect(secondaryBounds, clickX, clickY) && m_secondaryCallback) {
            m_secondaryCallback();
        }
    }
}

} // namespace UI
} // namespace Platform
} // namespace Engine

#endif // PLATFORM_HAS_NUKLEAR
