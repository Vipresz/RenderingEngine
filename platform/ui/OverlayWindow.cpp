#include "ui/OverlayWindow.h"

#include "render/Texture.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#include "nuklear.h"

#include <cmath>

namespace Engine {
namespace Platform {
namespace UI {

namespace {

constexpr int kOverlayFlags =
    NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_DYNAMIC;

struct nk_rect SnapUiRect(struct nk_rect bounds) {
    return nk_rect(
        std::floor(bounds.x + 0.5f),
        std::floor(bounds.y + 0.5f),
        std::floor(bounds.w + 0.5f),
        std::floor(bounds.h + 0.5f));
}

} // namespace

struct nk_rect OverlayScope::FullScreen(int screenWidth, int screenHeight) {
    return nk_rect(0.0f, 0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight));
}

OverlayScope::OverlayScope(struct nk_context* ctx,
                           const char* title,
                           struct nk_rect bounds,
                           int extraFlags) {
    if (!ctx || bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    m_ctx = ctx;

    const struct nk_color transparent = nk_rgba(0, 0, 0, 0);
    const struct nk_style_item transparentBackground = nk_style_item_color(transparent);
    nk_style_push_style_item(m_ctx, &m_ctx->style.window.fixed_background, transparentBackground);
    nk_style_push_color(m_ctx, &m_ctx->style.window.background, transparent);
    nk_style_push_color(m_ctx, &m_ctx->style.window.border_color, transparent);
    nk_style_push_float(m_ctx, &m_ctx->style.window.border, 0.0f);
    nk_style_push_vec2(m_ctx, &m_ctx->style.window.padding, nk_vec2(0.0f, 0.0f));

    if (nk_begin(m_ctx, title, bounds, kOverlayFlags | extraFlags)) {
        m_open = true;
        m_canvas = nk_window_get_canvas(m_ctx);
    }
}

OverlayScope::~OverlayScope() {
    if (!m_ctx) {
        return;
    }

    nk_end(m_ctx);
    nk_style_pop_vec2(m_ctx);
    nk_style_pop_float(m_ctx);
    nk_style_pop_color(m_ctx);
    nk_style_pop_color(m_ctx);
    nk_style_pop_style_item(m_ctx);
}

struct nk_image TextureToNkImage(const Render::Texture& texture) {
    return nk_image_id(static_cast<int>(texture.GetID()));
}

void DrawTexture(struct nk_command_buffer* canvas,
                 const Render::Texture& texture,
                 struct nk_rect bounds,
                 struct nk_color tint) {
    if (!canvas || !texture.IsLoaded() || bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const struct nk_image image = TextureToNkImage(texture);
    nk_draw_image(canvas, SnapUiRect(bounds), &image, tint);
}

void DrawFilledCircle(struct nk_command_buffer* canvas,
                      float centerX,
                      float centerY,
                      float radius,
                      struct nk_color color) {
    if (!canvas || radius <= 0.0f) {
        return;
    }

    const struct nk_rect bounds =
        nk_rect(centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f);
    nk_fill_circle(canvas, bounds, color);
}

} // namespace UI
} // namespace Platform
} // namespace Engine
