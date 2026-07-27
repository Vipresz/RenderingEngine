#pragma once

#include "core/System.h"
#include "ui/MenuStyle.h"
#include "ui/FontLibrary.h"
#include <functional>
#include <memory>

struct nk_context;

namespace Engine {
namespace Platform {
class Window;
namespace UI {

class NuklearUI;

// Renders 2D menus on top of the 3D scene (priority 150, after RenderSystem)
class UISystem : public ECS::ISystem {
public:
    using UIBuilder = std::function<void(struct nk_context*, const MenuStyle&, int screenWidth, int screenHeight)>;

    UISystem();
    ~UISystem() override;

    bool Initialize(Window* window, const MenuStyle& style = MenuStyle::Astral());
    void Shutdown();

    void Initialize(ECS::Registry& registry) override;
    void Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) override;
    void Shutdown(ECS::Registry& registry) override;

    void SetUIBuilder(UIBuilder builder);
    void SetStyle(const MenuStyle& style);
    MenuStyle& GetStyle() { return m_style; }
    const MenuStyle& GetStyle() const { return m_style; }

    struct nk_context* GetContext();
    bool IsInitialized() const;

    FontLibrary& GetFontLibrary() { return m_fontLibrary; }
    const FontLibrary& GetFontLibrary() const { return m_fontLibrary; }

    // When true, game input (WASD, camera) should be suppressed
    void SetBlockingGameInput(bool block) { m_blockingGameInput = block; }
    bool IsBlockingGameInput() const { return m_blockingGameInput; }

    // Helper: centered menu button with theme sizing
    bool MenuButton(struct nk_context* ctx, const char* label, float width = 0.0f, float height = 0.0f) const;

private:
    std::unique_ptr<NuklearUI> m_nuklear;
    FontLibrary m_fontLibrary;
    MenuStyle m_style;
    UIBuilder m_builder;
    Window* m_window = nullptr;
    bool m_blockingGameInput = false;
    bool m_styleApplied = false;
};

} // namespace UI
} // namespace Platform
} // namespace Engine
