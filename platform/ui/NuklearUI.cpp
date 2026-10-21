#include "NuklearUI.h"
#include "ui/FontLibrary.h"
#include "../Logger.h"
#include <cstring>

// Include Nuklear headers (without implementation - that's in NuklearImpl.cpp)
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
// NOTE: Do NOT define NK_IMPLEMENTATION or NK_GLFW_GL3_IMPLEMENTATION here (see NuklearImpl.cpp)
// Use NUKLEAR_DIR include path set by parent CMake
#include "nuklear.h"
#include "demo/glfw_opengl3/nuklear_glfw_gl3.h"

using EngineLogging::Logger;

namespace Engine {
namespace Platform {
namespace UI {

NuklearUI::NuklearUI()
    : m_nuklear(nullptr)
    , m_nkContext(nullptr)
    , m_nuklearInitialized(false)
    , m_window(nullptr)
{
}

NuklearUI::~NuklearUI() {
    Shutdown();
}

bool NuklearUI::Initialize(GLFWwindow* window, FontLibrary* fontLibrary) {
    if (!window) {
        LOG_ERROR("NuklearUI", "No GLFW window context for Nuklear");
        return false;
    }
    
    m_window = window;
    m_fontLibrary = fontLibrary;
    m_nuklear = new struct nk_glfw();
    memset(m_nuklear, 0, sizeof(struct nk_glfw));
    
    // Save the original user pointer (set by Window class for its key callbacks)
    void* originalUserPointer = glfwGetWindowUserPointer(m_window);
    
    // Initialize WITHOUT callbacks - we'll manually feed only mouse input
    // This prevents Nuklear from capturing keyboard input (WASD)
    // NOTE: nk_glfw3_init() sets the window user pointer to its own context,
    // but since we use NK_GLFW3_DEFAULT (no callbacks), we can restore the
    // original user pointer after init.
    m_nkContext = nk_glfw3_init(m_nuklear, m_window, NK_GLFW3_DEFAULT);
    if (!m_nkContext) {
        LOG_ERROR("NuklearUI", "Failed to initialize Nuklear context");
        delete m_nuklear;
        m_nuklear = nullptr;
        // Restore original user pointer even on failure
        glfwSetWindowUserPointer(m_window, originalUserPointer);
        return false;
    }
    
    // Restore the original user pointer so Window's key callbacks work correctly
    glfwSetWindowUserPointer(m_window, originalUserPointer);
    
    struct nk_font_atlas* atlas;
    nk_glfw3_font_stash_begin(m_nuklear, &atlas);
    // The editor has no font library. Leave the atlas empty so the bake installs
    // Nuklear's 13px font and sets default_font. A pre-added 36px font skips that
    // and, once selected, overflows the 18-30px rows.
    // A font library still bakes its HUD face. That makes font_num non-zero, so
    // default_font must be set here or the first window reads a null style font.
    struct nk_font* defaultPlayFont = nullptr;
    if (m_fontLibrary) {
#ifdef NK_INCLUDE_DEFAULT_FONT
        struct nk_font_config playFontCfg = nk_font_config(m_fontLibrary->GetConfig().hudSize);
        playFontCfg.range = nk_font_default_glyph_ranges();
        playFontCfg.oversample_h = 1;
        playFontCfg.oversample_v = 1;
        defaultPlayFont = nk_font_atlas_add_default(atlas, m_fontLibrary->GetConfig().hudSize, &playFontCfg);
        if (defaultPlayFont) {
            atlas->default_font = defaultPlayFont;
            m_fontLibrary->SetBakedFont(Engine::Platform::UI::FontRole::Hud, defaultPlayFont);
            m_fontLibrary->SetBakedFont(Engine::Platform::UI::FontRole::UiBold, defaultPlayFont);
            m_fontLibrary->SetBakedFont(Engine::Platform::UI::FontRole::UiRegular, defaultPlayFont);
        }
#endif
        m_fontLibrary->LoadChatFont(atlas);
    }
    nk_glfw3_font_stash_end(m_nuklear);
    if (m_fontLibrary) {
        m_fontLibrary->ApplyDefaultFont(m_nkContext);
    }
    if (!m_nkContext->style.font && defaultPlayFont) {
        nk_style_set_font(m_nkContext, &defaultPlayFont->handle);
    }
    if (!m_nkContext->style.font) {
        LOG_ERROR("NuklearUI", "Nuklear style font is null; the first window would crash");
        nk_glfw3_shutdown(m_nuklear);
        delete m_nuklear;
        m_nuklear = nullptr;
        m_nkContext = nullptr;
        return false;
    }

    m_nuklearInitialized = true;
    LOG_INFO("NuklearUI", m_fontLibrary && m_fontLibrary->HasAnyCustomFont()
        ? "Nuklear initialized with custom fonts"
        : "Nuklear initialized successfully (without keyboard callbacks)");
    return true;
}

void NuklearUI::Shutdown() {
    if (m_nuklearInitialized && m_nuklear) {
        nk_glfw3_shutdown(m_nuklear);
        delete m_nuklear;
        m_nuklear = nullptr;
        m_nkContext = nullptr;
        m_nuklearInitialized = false;
    }
}

void NuklearUI::BeginFrame() {
    if (m_nuklearInitialized && m_nuklear) {
        nk_glfw3_new_frame(m_nuklear);
    }
}

void NuklearUI::EndFrame() {
    if (m_nuklearInitialized && m_nuklear && m_window) {
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);

        if (width <= 0 || height <= 0) {
            return;
        }

        // Nuklear buffer sizes are in bytes, NOT screen pixels.
        constexpr int kMaxVertexBytes = 512 * 1024;
        constexpr int kMaxElementBytes = 128 * 1024;
        Render(kMaxVertexBytes, kMaxElementBytes);
    }
}

void NuklearUI::Render(int maxVertexBuffer, int maxElementBuffer) {
    if (m_nuklearInitialized && m_nuklear && m_window) {
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);
        
        if (width <= 0 || height <= 0) return;
        
        // Set viewport to match framebuffer size
        glViewport(0, 0, width, height);
        
        // Set up OpenGL state for UI rendering
        // Nuklear needs blending enabled and depth testing disabled for proper UI rendering
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);  // Ensure scissor test is disabled
        
        // Render Nuklear UI with specified buffer sizes
        nk_glfw3_render(m_nuklear, NK_ANTI_ALIASING_ON, maxVertexBuffer, maxElementBuffer);
        
        // Restore OpenGL state (Nuklear may have changed these)
        glDisable(GL_SCISSOR_TEST);
        glUseProgram(0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }
}

} // namespace UI
} // namespace Platform
} // namespace Engine

