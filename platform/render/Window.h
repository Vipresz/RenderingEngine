#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace Engine {
namespace Platform {

class Window {
public:
    Window();
    Window(GLint windowWidth, GLint windowHeight);
    
    int Initialize();
    
    bool getShouldClose() { return glfwWindowShouldClose(mainWindow); }
    void setShouldClose(bool value) { glfwSetWindowShouldClose(mainWindow, value ? GLFW_TRUE : GLFW_FALSE); }
    
    bool* getsKeys() { return keys; }
    bool isMouseButtonDown(int glfwMouseButton) const;
    void SetCursorCaptured(bool captured);
    
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
    
    void createCallbacks();
    static void handleKeys(GLFWwindow* window, int key, int code, int action, int mode);
    static void handleMouse(GLFWwindow* window, double xPos, double yPos);
    static void handleMouseButtons(GLFWwindow* window, int button, int action, int mods);
    static void handleScroll(GLFWwindow* window, double xOffset, double yOffset);
};

} // namespace Platform
} // namespace Engine



