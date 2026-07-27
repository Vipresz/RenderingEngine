#include "ui/UISystem.h"
#include "ui/NuklearUI.h"
#include "ui/FontLibrary.h"
#include "render/Window.h"
#include "Logger.h"

#include <GLFW/glfw3.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "nuklear.h"

namespace Engine {
namespace Platform {
namespace UI {

UISystem::UISystem() {
    SetPriority(150); // After RenderSystem (100)
    m_nuklear = std::make_unique<NuklearUI>();
}

UISystem::~UISystem() {
    Shutdown();
}

bool UISystem::Initialize(Window* window, const MenuStyle& style) {
    m_window = window;
    m_style = style;
    if (!window || !window->GetGLFWWindow()) {
        LOG_ERROR("UISystem", "Invalid window for UI initialization");
        return false;
    }

    FontLibrary::Config fontConfig;
    fontConfig.uiSize = style.metrics.fontSize;
    fontConfig.uiSmallSize = style.metrics.fontSize * 0.78f;
    fontConfig.monoSize = style.metrics.monoFontSize;
    fontConfig.hudSize = style.metrics.hudFontSize;
    if (!style.fontsRoot.empty()) {
        fontConfig.fontsRoot = style.fontsRoot;
    }
    m_fontLibrary.SetConfig(fontConfig);

    if (!m_nuklear->Initialize(window->GetGLFWWindow(), &m_fontLibrary)) {
        return false;
    }
    m_style.Apply(m_nuklear->GetContext());
    m_fontLibrary.ApplyDefaultFont(m_nuklear->GetContext());
    m_styleApplied = true;
    LOG_INFO("UISystem", "UI system initialized");
    return true;
}

void UISystem::Shutdown() {
    if (m_nuklear) {
        m_nuklear->Shutdown();
    }
    m_window = nullptr;
    m_styleApplied = false;
}

void UISystem::Initialize(ECS::Registry& /*registry*/) {}

void UISystem::Update(float /*deltaTime*/, ECS::Registry& /*registry*/, uint64_t /*tickNumber*/) {
    if (!m_nuklear || !m_nuklear->IsInitialized()) {
        return;
    }

    if (!m_styleApplied) {
        m_style.Apply(m_nuklear->GetContext());
        m_styleApplied = true;
    }

    m_nuklear->BeginFrame();

    if (m_builder) {
        // Nuklear's GLFW backend lays out in window (logical) pixels and scales to the
        // framebuffer via fb_scale. Using framebuffer size here causes blur on Windows DPI.
        int screenW = 0;
        int screenH = 0;
        if (m_window && m_window->GetGLFWWindow()) {
            glfwGetWindowSize(m_window->GetGLFWWindow(), &screenW, &screenH);
        }
        if (screenW <= 0 || screenH <= 0) {
            screenW = m_window ? m_window->getBufferWidth() : 0;
            screenH = m_window ? m_window->getBufferHeight() : 0;
        }
        m_builder(m_nuklear->GetContext(), m_style, screenW, screenH);
    }

    m_nuklear->EndFrame();
}

void UISystem::Shutdown(ECS::Registry& /*registry*/) {
    Shutdown();
}

void UISystem::SetUIBuilder(UIBuilder builder) {
    m_builder = std::move(builder);
}

void UISystem::SetStyle(const MenuStyle& style) {
    m_style = style;
    m_styleApplied = false;
    if (m_nuklear && m_nuklear->IsInitialized()) {
        m_style.Apply(m_nuklear->GetContext());
        m_styleApplied = true;
    }
}

struct nk_context* UISystem::GetContext() {
    return m_nuklear ? m_nuklear->GetContext() : nullptr;
}

bool UISystem::IsInitialized() const {
    return m_nuklear && m_nuklear->IsInitialized();
}

bool UISystem::MenuButton(struct nk_context* ctx, const char* label, float width, float height) const {
    if (!ctx || !label) return false;
    const float w = (width > 0.0f) ? width : m_style.metrics.menuButtonWidth;
    const float h = (height > 0.0f) ? height : m_style.metrics.menuButtonHeight;
    nk_layout_row_static(ctx, h, static_cast<int>(w), 1);
    return nk_button_label(ctx, label) == nk_true;
}

} // namespace UI
} // namespace Platform
} // namespace Engine
