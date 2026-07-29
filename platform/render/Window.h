#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <functional>

namespace Engine {
namespace Platform {

class Window {
public:
    using EscapeHandler = std::function<bool()>;
    using CharInputHandler = std::function<void(unsigned int codepoint)>;

    Window();
    Window(GLint windowWidth, GLint windowHeight);
    
    int Initialize();
    
    bool getShouldClose() { return glfwWindowShouldClose(mainWindow); }
    void setShouldClose(bool value) { glfwSetWindowShouldClose(mainWindow, value ? GLFW_TRUE : GLFW_FALSE); }
    
    bool* getsKeys() { return keys; }
    bool isMouseButtonDown(int glfwMouseButton) const;
    void SetCursorCaptured(bool captured);
    void SetEscapeHandler(EscapeHandler handler) { m_escapeHandler = std::move(handler); }
    void SetCharInputHandler(CharInputHandler handler) { m_charInputHandler = std::move(handler); }
    
    GLfloat getXChange();
    GLfloat getYChange();
    GLfloat getScrollYChange(); // Get mouse wheel scroll delta
    
    void swapBuffers() { glfwSwapBuffers(mainWindow); }
    void pollEvents();
    void updateFramebufferSize();

    GLint getWindowWidth() const { return width; }
    GLint getWindowHeight() const { return height; }
    GLint getBufferWidth() const { return bufferWidth; }
    GLint getBufferHeight() const { return bufferHeight; }

    GLFWwindow* GetGLFWWindow() const { return mainWindow; }
    
    ~Window();

private:
    GLFWwindow* mainWindow;
    
    GLint width, height;
    GLint bufferWidth, bufferHeight;
    
    bool keys[1024];
    bool mouseButtons[8];
    
    bool cursorCaptured = false;
    GLfloat lastX;
    GLfloat lastY;
    GLfloat xChange;
    GLfloat yChange;
    GLfloat scrollYChange;
    bool mouseFirstMoved;
    EscapeHandler m_escapeHandler;
    CharInputHandler m_charInputHandler;
    
    void createCallbacks();
    static void handleChars(GLFWwindow* window, unsigned int codepoint);
    static void handleKeys(GLFWwindow* window, int key, int code, int action, int mode);
    static void handleMouse(GLFWwindow* window, double xPos, double yPos);
    static void handleMouseButtons(GLFWwindow* window, int button, int action, int mods);
    static void handleScroll(GLFWwindow* window, double xOffset, double yOffset);
};

} // namespace Platform
} // namespace Engine



