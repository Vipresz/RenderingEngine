#include "TestRuntime.h"

#include <gtest/gtest.h>

#include "render/Camera.h"
#include "render/RenderPipeline.h"
#include "render/Shader.h"
#include "render/Window.h"
#include "render/passes/OpaqueRenderPass.h"
#include "render/passes/ShadowRenderPass.h"
#include "render/passes/TransparentRenderPass.h"
#include "core/ecs/Registry.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

#include <filesystem>
#include <memory>

using Engine::ECS::Registry;
using Engine::Platform::Render::Camera;
using Engine::Platform::Render::RenderContext;
using Engine::Platform::Render::RenderPipeline;
using Engine::Platform::Render::Shader;
using Engine::Platform::Render::Passes::OpaqueRenderPass;
using Engine::Platform::Render::Passes::ShadowRenderPass;
using Engine::Platform::Render::Passes::TransparentRenderPass;
using Engine::Platform::Window;

namespace {

constexpr const char* kMinimalVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 position;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
void main() {
    gl_Position = projection * view * model * vec4(position, 1.0);
}
)";

constexpr const char* kMinimalFragmentShader = R"(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
)";

void ExpectNoGlError(const char* stage) {
    const GLenum error = glGetError();
    EXPECT_EQ(error, static_cast<GLenum>(GL_NO_ERROR)) << "OpenGL error after " << stage;
}

void EnsureShaderWorkingDirectory() {
    Engine::Tests::SetWorkingDirectoryToExecutable();
}

} // namespace

class RenderPipelinePassEnvironment : public ::testing::Test {
protected:
    Window window{64, 64};
    Registry registry;
    Shader shader;
    Camera camera;
    RenderPipeline pipeline;

    void SetUp() override {
        ASSERT_EQ(window.Initialize(), 0);
        EnsureShaderWorkingDirectory();
        shader.CreateFromString(kMinimalVertexShader, kMinimalFragmentShader);
        ASSERT_TRUE(shader.IsValid());

        pipeline.AddPass(std::make_unique<ShadowRenderPass>());
        pipeline.AddPass(std::make_unique<OpaqueRenderPass>());
        pipeline.AddPass(std::make_unique<TransparentRenderPass>());
    }
};

TEST_F(RenderPipelinePassEnvironment, ExecutesShadowOpaqueAndTransparentPasses) {
    RenderContext context;
    context.registry = &registry;
    context.shader = &shader;
    context.cameraSource = &camera;
    context.projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    context.view = camera.GetViewMatrix();
    context.viewProjection = context.projection * context.view;
    context.deltaTime = 1.0f / 60.0f;
    context.tickNumber = 1;

    glViewport(0, 0, 64, 64);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    pipeline.Execute(context);
    ExpectNoGlError("RenderPipeline.Execute");
}

TEST_F(RenderPipelinePassEnvironment, ShadowPassReportsAvailability) {
    ShadowRenderPass shadow;
    RenderContext context;
    context.registry = &registry;
    context.shader = &shader;
    context.cameraSource = &camera;
    context.projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    context.view = camera.GetViewMatrix();
    context.viewProjection = context.projection * context.view;

    shadow.Execute(context);
    ExpectNoGlError("ShadowRenderPass.Execute");
}

TEST_F(RenderPipelinePassEnvironment, TransparentPassHandlesEmptyScene) {
    TransparentRenderPass transparent;
    RenderContext context;
    context.registry = &registry;
    context.shader = &shader;
    context.cameraSource = &camera;
    context.projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    context.view = camera.GetViewMatrix();
    context.viewProjection = context.projection * context.view;

    transparent.Execute(context);
    ExpectNoGlError("TransparentRenderPass.Execute");
}
