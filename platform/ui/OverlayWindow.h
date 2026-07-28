#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

struct nk_context;
struct nk_rect;
struct nk_command_buffer;
struct nk_image;
struct nk_color;

namespace Engine {
namespace Platform {
namespace Render {
class Texture;
}
namespace UI {

// Transparent Nuklear overlay window for HUD/menus drawn over the 3D scene.
class OverlayScope {
public:
    OverlayScope(struct nk_context* ctx,
                 const char* title,
                 struct nk_rect bounds,
                 int extraFlags = 0);
    ~OverlayScope();

    OverlayScope(const OverlayScope&) = delete;
    OverlayScope& operator=(const OverlayScope&) = delete;

    bool IsOpen() const { return m_open; }
    struct nk_command_buffer* Canvas() const { return m_canvas; }

    static struct nk_rect FullScreen(int screenWidth, int screenHeight);

private:
    struct nk_context* m_ctx = nullptr;
    struct nk_command_buffer* m_canvas = nullptr;
    bool m_open = false;
};

struct nk_image TextureToNkImage(const Render::Texture& texture);
void DrawTexture(struct nk_command_buffer* canvas,
                 const Render::Texture& texture,
                 struct nk_rect bounds,
                 struct nk_color tint);
void DrawFilledCircle(struct nk_command_buffer* canvas,
                      float centerX,
                      float centerY,
                      float radius,
                      struct nk_color color);

} // namespace UI
} // namespace Platform
} // namespace Engine

#endif // PLATFORM_HAS_NUKLEAR
