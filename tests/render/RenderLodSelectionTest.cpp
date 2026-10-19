#include <gtest/gtest.h>

#include "render/Model.h"
#include "render/RenderLodSelection.h"
#include "render/components/MeshComponent.h"
#include "render/components/MeshLodComponent.h"
#include "render/components/TransformComponent.h"

using Engine::Platform::Render::Components::MeshComponent;
using Engine::Platform::Render::Components::MeshLodComponent;
using Engine::Platform::Render::Components::MeshLodLevel;
using Engine::Platform::Render::Components::TransformComponent;
using Engine::Platform::Render::SelectLodModel;

namespace {

Engine::Platform::Render::Model nearModel;
Engine::Platform::Render::Model farModel;
Engine::Platform::Render::Model fallbackModel;

} // namespace

TEST(RenderLodSelectionTest, UsesBaseMeshWhenNoLodComponent) {
    MeshComponent mesh;
    mesh.model = &nearModel;

    EXPECT_EQ(SelectLodModel(mesh, nullptr, 100.0f), &nearModel);
}

TEST(RenderLodSelectionTest, SelectsNearestLodWithinDistance) {
    MeshComponent mesh;
    mesh.model = &fallbackModel;

    MeshLodComponent lod;
    lod.levels = {
        MeshLodLevel{&nearModel, 20.0f},
        MeshLodLevel{&farModel, 80.0f},
    };

    EXPECT_EQ(SelectLodModel(mesh, &lod, 10.0f), &nearModel);
    EXPECT_EQ(SelectLodModel(mesh, &lod, 50.0f), &farModel);
    EXPECT_EQ(SelectLodModel(mesh, &lod, 200.0f), &farModel);
}
