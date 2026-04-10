#include "render/Window.h"
#include "../Logger.h"
#include <cstdio>
#include <cstring>

namespace Engine {
namespace Platform {

using EngineLogging::Logger;

// Last GLFW error from callback (glfwGetError may be empty after the callback runs)
static int s_lastGlfwCallbackCode = GLFW_NO_ERROR;
static char s_lastGlfwCallbackDesc[768] = {};

static void glfw_error_callback(int error, const char* description) {
    s_lastGlfwCallbackCode = error;
    s_lastGlfwCallbackDesc[0] = '\0';
    if (description) {
#if defined(_MSC_VER)
        strncpy_s(s_lastGlfwCallbackDesc, sizeof(s_lastGlfwCallbackDesc), description, _TRUNCATE);
#else
        std::strncpy(s_lastGlfwCallbackDesc, description, sizeof(s_lastGlfwCallbackDesc) - 1);
        s_lastGlfwCallbackDesc[sizeof(s_lastGlfwCallbackDesc) - 1] = '\0';
#endif
    }
    // Do not call Logger or iostream from here — GLFW invokes this on the same thread during
    // glfwCreateWindow; logging can re-enter CRT/GPU paths and crash or deadlock on some drivers.
    const bool probeNoise = (error == 65542 || error == 65543);
    if (!probeNoise) {
        std::fprintf(stderr, "GLFW Error %d: %s\n", error, description ? description : "");
        std::fflush(stderr);
    }
}

Window::Window() : mainWindow(nullptr), width(800), height(600), bufferWidth(0), bufferHeight(0),
    lastX(0), lastY(0), xChange(0.0f), yChange(0.0f), scrollYChange(0.0f), mouseFirstMoved(true) {
    for (size_t i = 0; i < 1024; i++) keys[i] = false;
}

Window::Window(GLint windowWidth, GLint windowHeight) : mainWindow(nullptr), width(windowWidth), height(windowHeight),
    bufferWidth(0), bufferHeight(0), lastX(0), lastY(0), xChange(0.0f), yChange(0.0f), scrollYChange(0.0f), mouseFirstMoved(true) {
    for (size_t i = 0; i < 1024; i++) keys[i] = false;
}

int Window::Initialize() {
    s_lastGlfwCallbackCode = GLFW_NO_ERROR;
    s_lastGlfwCallbackDesc[0] = '\0';

#if !defined(_WIN32)
    // Windows: do not install a GLFW error callback until after glfwCreateWindow succeeds — some
    // WGL stacks recurse into the error path; a user callback contributed to stack overflow (0xC00000FD).
    glfwSetErrorCallback(glfw_error_callback);
#endif

    if (!glfwInit()) {
        LOG_ERROR("Window", "glfwInit() failed");
        glfwTerminate();
        return 1;
    }
    {
        int mj = 0, mn = 0, rv = 0;
        glfwGetVersion(&mj, &mn, &rv);
        LOG_INFO_FMT("Window", "GLFW %d.%d.%d (%s)", mj, mn, rv, glfwGetVersionString() ? glfwGetVersionString() : "");
    }

#ifdef __APPLE__
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    mainWindow = glfwCreateWindow(width, height, "Camera System Editor", nullptr, nullptr);
#else
    // Non-macOS: hidden window; reset hints between attempts.
    // Windows: if WGL lacks WGL_ARB_create_context_profile (65543), do not set GLFW_OPENGL_PROFILE.
    // If only GL 1.1 is available (Microsoft ICD), omit profile and fall back to legacy hints.
    struct GlTry {
        int major;
        int minor;
        bool set_profile;
        int profile;
        bool double_buffer; // GLFW default is double-buffered; GDI / some drivers only expose single-buffer GL
        const char* label;
    };
# if defined(_WIN32)
    // Prefer GL 3.3+ first (editor shaders require it). 1.1 last — GDI fallback only if nothing else works.
    static const GlTry kTries[] = {
        {3, 3, true, GLFW_OPENGL_COMPAT_PROFILE, true, "3.3 compat"},
        {3, 3, true, GLFW_OPENGL_ANY_PROFILE, true, "3.3 any"},
        {2, 1, true, GLFW_OPENGL_ANY_PROFILE, true, "2.1 any"},
        {3, 3, false, 0, true, "3.3 no profile (legacy WGL)"},
        {2, 1, false, 0, true, "2.1 no profile"},
        {3, 3, true, GLFW_OPENGL_COMPAT_PROFILE, false, "3.3 compat (single-buffer)"},
        {3, 3, true, GLFW_OPENGL_ANY_PROFILE, false, "3.3 any (single-buffer)"},
        {2, 1, true, GLFW_OPENGL_ANY_PROFILE, false, "2.1 any (single-buffer)"},
        {3, 3, false, 0, false, "3.3 no profile (single-buffer)"},
        {2, 1, false, 0, false, "2.1 no profile (single-buffer)"},
        {1, 1, false, 0, true, "1.1 legacy"},
        {1, 1, false, 0, false, "1.1 legacy (single-buffer)"},
    };
# else
    static const GlTry kTries[] = {
        {3, 3, true, GLFW_OPENGL_COMPAT_PROFILE, true, "3.3 compat"},
    };
# endif
    mainWindow = nullptr;
# if defined(_WIN32)
    int lastProbeCode = GLFW_NO_ERROR;
    const char* lastProbeDesc = nullptr;
    glfwSetErrorCallback(nullptr);
#endif
    for (const GlTry& t : kTries) {
#if defined(_WIN32)
        while (glfwGetError(nullptr) != GLFW_NO_ERROR) {
        }
#endif
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_DOUBLEBUFFER, t.double_buffer ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, t.major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, t.minor);
        if (t.set_profile) {
            glfwWindowHint(GLFW_OPENGL_PROFILE, t.profile);
        }
        mainWindow = glfwCreateWindow(width, height, "Camera System Editor", nullptr, nullptr);
        if (mainWindow) {
            LOG_INFO_FMT("Window", "OpenGL context: %s", t.label);
            break;
        }
#if defined(_WIN32)
        {
            const char* d = nullptr;
            const int e = glfwGetError(&d);
            lastProbeCode = e;
            lastProbeDesc = d;
            if (e != GLFW_NO_ERROR && e != 0) {
                if (e == 65542 || e == 65543) {
                    LOG_WARNING_FMT("Window", "WGL try \"%s\": %d %s", t.label, e, d ? d : "");
                } else {
                    LOG_WARNING_FMT("Window", "Window try \"%s\": GLFW %d %s", t.label, e, d ? d : "");
                }
            }
        }
#endif
    }
#if defined(_WIN32)
    glfwSetErrorCallback(glfw_error_callback);
#endif
#endif

    if (!mainWindow) {
        const char* errDesc = nullptr;
        int errCode = glfwGetError(&errDesc);
#if defined(_WIN32)
        if ((errCode == GLFW_NO_ERROR || errCode == 0) && (lastProbeCode != GLFW_NO_ERROR && lastProbeCode != 0)) {
            errCode = lastProbeCode;
            errDesc = lastProbeDesc;
        }
#else
        if ((errCode == GLFW_NO_ERROR || errCode == 0) && s_lastGlfwCallbackCode != GLFW_NO_ERROR) {
            errCode = s_lastGlfwCallbackCode;
            errDesc = s_lastGlfwCallbackDesc;
        }
#endif
        LOG_ERROR_FMT("Window", "glfwCreateWindow failed (code %d): %s", errCode,
            errDesc && errDesc[0] ? errDesc : "(no description)");
        if (errCode == 65542) {
            LOG_ERROR("Window",
                "WGL: no usable pixel format (65542). Update GPU drivers; assign discrete GPU to this exe; "
                "install OpenGL Compatibility Pack (Store). If only GDI Generic exists, ensure "
                "CAMERA_SYSTEM_ALLOW_WGL_NO_ACCEL=1 (default) and try single-buffer fallbacks above.");
        } else if (errCode == 65543) {
            LOG_ERROR("Window",
                "WGL: requested GL version/profile not available (65543). Often fixed by GPU drivers or "
                "Compatibility Pack; this build also tries legacy contexts without ARB profile hints.");
        }
        glfwTerminate();
        return 1;
    }

    glfwGetFramebufferSize(mainWindow, &bufferWidth, &bufferHeight);

    glfwMakeContextCurrent(mainWindow);

    createCallbacks();
    glfwSetInputMode(mainWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glewExperimental = GL_TRUE;

    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        LOG_ERROR_FMT("Window", "glewInit() failed: %s", glewGetErrorString(glewErr));
        glfwDestroyWindow(mainWindow);
        glfwTerminate();
        return 1;
    }

    while (glGetError() != GL_NO_ERROR) {
    }

    glfwSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, bufferWidth, bufferHeight);

    glfwSetWindowUserPointer(mainWindow, this);

    glfwShowWindow(mainWindow);
    glfwFocusWindow(mainWindow);

    return 0;
}

void Window::createCallbacks() {
    glfwSetKeyCallback(mainWindow, handleKeys);
    glfwSetCursorPosCallback(mainWindow, handleMouse);
    glfwSetScrollCallback(mainWindow, handleScroll);
}

GLfloat Window::getXChange() {
    GLfloat theChange = xChange;
    xChange = 0.0f;
    return theChange;
}

GLfloat Window::getYChange() {
    GLfloat theChange = yChange;
    yChange = 0.0f;
    return theChange;
}

void Window::handleKeys(GLFWwindow* window, int key, int code, int action, int mode) {
    Window* theWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) {
            theWindow->keys[key] = true;
        } else if (action == GLFW_RELEASE) {
            theWindow->keys[key] = false;
        }
    }
}

void Window::handleMouse(GLFWwindow* window, double xPos, double yPos) {
    Window* theWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));

    if (theWindow->mouseFirstMoved) {
        theWindow->lastX = xPos;
        theWindow->lastY = yPos;
        theWindow->mouseFirstMoved = false;
    }

    theWindow->xChange = xPos - theWindow->lastX;
    theWindow->yChange = theWindow->lastY - yPos;

    theWindow->lastX = xPos;
    theWindow->lastY = yPos;
}

GLfloat Window::getScrollYChange() {
    GLfloat theChange = scrollYChange;
    scrollYChange = 0.0f;
    return theChange;
}

void Window::handleScroll(GLFWwindow* window, double xOffset, double yOffset) {
    Window* theWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));
    theWindow->scrollYChange = static_cast<GLfloat>(yOffset);
}

Window::~Window() {
    if (mainWindow) {
        glfwDestroyWindow(mainWindow);
        mainWindow = nullptr;
    }
    glfwTerminate();
}

} // namespace Platform
} // namespace Engine
