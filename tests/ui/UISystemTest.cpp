#ifdef PLATFORM_HAS_NUKLEAR

#include <gtest/gtest.h>

#include "render/Window.h"
#include "ui/MenuStyle.h"
#include "ui/UISystem.h"
#include "ui/UILayer.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "nuklear.h"

#include <vector>

using Engine::Platform::UI::UILayerId;
using Engine::Platform::UI::UILayerPriority;
using Engine::Platform::UI::UISystem;
using Engine::Platform::Window;

namespace {

std::vector<int16_t> g_drawOrder;

} // namespace

class UISystemEnvironment : public ::testing::Test {
protected:
    Window window{128, 128};
    UISystem ui;

    void SetUp() override {
        ASSERT_EQ(window.Initialize(), 0);
        ASSERT_TRUE(ui.Initialize(&window, Engine::Platform::UI::MenuStyle::Astral()));
        g_drawOrder.clear();
    }

    void TearDown() override {
        ui.Shutdown();
    }
};

TEST_F(UISystemEnvironment, InitializesAndPresentsEmptyFrame) {
    EXPECT_TRUE(ui.IsInitialized());
    ui.PresentFrame();
}

TEST_F(UISystemEnvironment, LayersDrawInPriorityOrder) {
    ui.RegisterLayer(UILayerId::Panels, [](struct nk_context*, const Engine::Platform::UI::MenuStyle&, int, int) {
        g_drawOrder.push_back(UILayerPriority(UILayerId::Panels));
    });
    ui.RegisterLayer(UILayerId::Hud, [](struct nk_context*, const Engine::Platform::UI::MenuStyle&, int, int) {
        g_drawOrder.push_back(UILayerPriority(UILayerId::Hud));
    });
    ui.RegisterLayer(UILayerId::Popups, [](struct nk_context*, const Engine::Platform::UI::MenuStyle&, int, int) {
        g_drawOrder.push_back(UILayerPriority(UILayerId::Popups));
    });

    ASSERT_TRUE(ui.HasRegisteredLayers());
    ui.PresentFrame();

    ASSERT_EQ(g_drawOrder.size(), 3u);
    EXPECT_LT(g_drawOrder[0], g_drawOrder[1]);
    EXPECT_LT(g_drawOrder[1], g_drawOrder[2]);
}

TEST_F(UISystemEnvironment, ClearLayersRemovesCallbacks) {
    ui.RegisterLayer(UILayerId::Hud, [](struct nk_context*, const Engine::Platform::UI::MenuStyle&, int, int) {
        g_drawOrder.push_back(1);
    });
    ui.ClearLayers();
    EXPECT_FALSE(ui.HasRegisteredLayers());
    ui.PresentFrame();
    EXPECT_TRUE(g_drawOrder.empty());
}

#endif // PLATFORM_HAS_NUKLEAR
