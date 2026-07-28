#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Forward declarations
struct nk_glfw;
struct nk_context;

namespace Engine {
namespace Platform {
namespace UI {

class FontLibrary;

class NuklearUI {
public:
    NuklearUI();
    ~NuklearUI();
    
    // Initialize Nuklear (optional custom fonts baked from menu/fonts)
    bool Initialize(GLFWwindow* window, FontLibrary* fontLibrary = nullptr);
    
    FontLibrary* GetFontLibrary() { return m_fontLibrary; }
    const FontLibrary* GetFontLibrary() const { return m_fontLibrary; }
    
    // Shutdown Nuklear
    void Shutdown();
    
    // Get Nuklear context
    struct nk_context* GetContext() { return m_nkContext; }
    const struct nk_context* GetContext() const { return m_nkContext; }
    
    // Check if initialized
    bool IsInitialized() const { return m_nuklearInitialized; }
    
    // Begin/end frame
    void BeginFrame();
    void EndFrame();
    
    // Render with custom buffer sizes (matches old version behavior)
    void Render(int maxVertexBuffer, int maxElementBuffer);
    
    // Get nuklear instance for direct rendering (matches old version)
    struct nk_glfw* GetNuklear() { return m_nuklear; }
    
private:
    struct nk_glfw* m_nuklear;
    struct nk_context* m_nkContext;
    bool m_nuklearInitialized;
    GLFWwindow* m_window;
    FontLibrary* m_fontLibrary = nullptr;
};

} // namespace UI
} // namespace Platform
} // namespace Engine

