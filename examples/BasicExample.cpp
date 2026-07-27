// Engine Example - Demonstrates rendering and ECS

#include "platform/Logger.h"
#include "core/ecs/Registry.h"
#include "core/System.h"
#include "render/Window.h"
#include "render/Shader.h"
#include "render/Model.h"
#include "render/FollowCamera.h"
#include "render/components/TransformComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/components/AnimationComponent.h"
#include "render/systems/RenderSystem.h"
#include "render/systems/CameraSystem.h"
#include "input/InputSystem.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <cmath>
#include <fstream>
#include <filesystem>

using namespace Engine;
using namespace Engine::ECS;
using namespace Engine::Platform;
using namespace Engine::Platform::Render;
using namespace Engine::Platform::Render::Components;
using namespace Engine::Platform::Render::Systems;
using namespace Engine::Platform::Input::Systems;

GLuint CreateMetallicTextureGL() {
    const int width = 256;
    const int height = 256;
    unsigned char* data = new unsigned char[width * height * 3];
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index = (y * width + x) * 3;
            int patternValue = (x + static_cast<int>(y * 0.1f)) % 20;
            float noise = static_cast<float>(patternValue) / 20.0f;
            float intensity = 0.5f + 0.3f * noise;
            
            data[index] = static_cast<unsigned char>(intensity * 180);
            data[index + 1] = static_cast<unsigned char>(intensity * 200);
            data[index + 2] = static_cast<unsigned char>(intensity * 220);
        }
    }
    
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    delete[] data;
    
    return textureID;
}

int main() {
    EngineLogging::Logger& logger = EngineLogging::Logger::GetInstance();
    logger.Initialize("basic_example.log");
    logger.SetMinLevel(EngineLogging::LogLevel::LevelInfo);
    
    try {
        Window mainWindow(1280, 720);
        if (mainWindow.Initialize() != 0) {
            LOG_ERROR("BasicExample", "Failed to initialize window");
            return 1;
        }
        
        GLuint metallicTexture = CreateMetallicTextureGL();
        
        Shader shader;
        std::string vertPath, fragPath;
        if (std::filesystem::exists("shaders/shader.vert") && std::filesystem::exists("shaders/shader.frag")) {
            vertPath = "shaders/shader.vert";
            fragPath = "shaders/shader.frag";
        } else if (std::filesystem::exists("bin/Release/shaders/shader.vert") && std::filesystem::exists("bin/Release/shaders/shader.frag")) {
            vertPath = "bin/Release/shaders/shader.vert";
            fragPath = "bin/Release/shaders/shader.frag";
        } else if (std::filesystem::exists("shader.vert") && std::filesystem::exists("shader.frag")) {
            vertPath = "shader.vert";
            fragPath = "shader.frag";
        } else {
            vertPath = "../../platform/render/shaders/shader.vert";
            fragPath = "../../platform/render/shaders/shader.frag";
        }
        shader.CreateFromFiles(vertPath.c_str(), fragPath.c_str());
        if (!shader.IsValid()) {
            LOG_ERROR("BasicExample", "Failed to compile shader");
            return 1;
        }
        shader.Validate();
        
        float aspectRatio = static_cast<float>(mainWindow.getBufferWidth()) / 
                           static_cast<float>(mainWindow.getBufferHeight());
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
        
        Registry registry;
        
        Entity ground = registry.CreateEntity();
        std::unique_ptr<Model> groundModel = std::make_unique<Model>();
        groundModel->CreatePlane(20.0f, 20.0f);
        
        auto& groundTransform = registry.AddComponent<TransformComponent>(ground.id);
        groundTransform.SetPosition(0.0f, 0.0f, 0.0f);
        groundTransform.SetScale(1.0f);
        
        auto& groundMesh = registry.AddComponent<MeshComponent>(ground.id);
        groundMesh.model = groundModel.get();
        
        auto& groundMaterial = registry.AddComponent<MaterialComponent>(ground.id);
        groundMaterial.specularIntensity = 0.2f;
        groundMaterial.shininess = 16.0f;
        
        Entity sphere = registry.CreateEntity();
        std::unique_ptr<Model> sphereModel = std::make_unique<Model>();
        sphereModel->CreateSphere(1.0f, 32);
        
        auto& sphereTransform = registry.AddComponent<TransformComponent>(sphere.id);
        sphereTransform.SetPosition(0.0f, 1.0f, 0.0f);
        sphereTransform.SetScale(1.0f);
        
        auto& sphereMesh = registry.AddComponent<MeshComponent>(sphere.id);
        sphereMesh.model = sphereModel.get();
        
        auto& sphereMaterial = registry.AddComponent<MaterialComponent>(sphere.id);
        sphereMaterial.specularIntensity = 0.8f;
        sphereMaterial.shininess = 128.0f;
        
        auto& sphereAnim = registry.AddComponent<AnimationComponent>(sphere.id);
        sphereAnim.groundLevel = 1.0f;
        sphereAnim.jumpInitialVelocity = 8.0f;
        
        FollowCamera followCamera;
        followCamera.SetTarget(sphere.id, &registry);
        followCamera.SetHeightOffset(1.0f);
        
        SystemManager systemManager;
        
        auto inputSystem = std::make_shared<InputSystem>(&mainWindow);
        inputSystem->SetCharacterEntity(sphere.id);
        inputSystem->SetMovementSpeed(5.0f);
        systemManager.RegisterSystem(inputSystem);

        auto cameraSystem = std::make_shared<CameraSystem>();
        cameraSystem->SetSource(&followCamera);
        systemManager.RegisterSystem(cameraSystem);
        
        auto renderSystem = std::make_shared<RenderSystem>(&shader, &followCamera, projection);
        systemManager.RegisterSystem(renderSystem);

        inputSystem->SetCameraSource(&followCamera);
        renderSystem->SetCameraSource(&followCamera);
        
        systemManager.Initialize(registry);
        
        float lastTime = static_cast<float>(glfwGetTime());
        
        while (!mainWindow.getShouldClose()) {
            float currentTime = static_cast<float>(glfwGetTime());
            float deltaTime = currentTime - lastTime;
            lastTime = currentTime;
            
            if (deltaTime > 0.1f) deltaTime = 0.1f;
            
            mainWindow.pollEvents();
            
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, metallicTexture);
            
            systemManager.Update(deltaTime, registry, 0);
            mainWindow.swapBuffers();
        }
        
        glDeleteTextures(1, &metallicTexture);
        groundModel->ClearModel();
        sphereModel->ClearModel();
        shader.ClearShader();
        systemManager.Shutdown(registry);
        
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("BasicExample", "Fatal error: %s", e.what());
        logger.Shutdown();
        return 1;
    }
    
    logger.Shutdown();
    return 0;
}
