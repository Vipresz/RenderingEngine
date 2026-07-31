#include <gtest/gtest.h>

#include "ui/UILayer.h"

using Engine::Platform::UI::UILayerId;
using Engine::Platform::UI::UILayerName;
using Engine::Platform::UI::UILayerPriority;

TEST(UILayerTest, PriorityIncreasesWithStackHeight) {
    EXPECT_LT(UILayerPriority(UILayerId::Background), UILayerPriority(UILayerId::Hud));
    EXPECT_LT(UILayerPriority(UILayerId::Hud), UILayerPriority(UILayerId::Prompts));
    EXPECT_LT(UILayerPriority(UILayerId::Prompts), UILayerPriority(UILayerId::Panels));
    EXPECT_LT(UILayerPriority(UILayerId::Panels), UILayerPriority(UILayerId::Popups));
    EXPECT_LT(UILayerPriority(UILayerId::Popups), UILayerPriority(UILayerId::Debug));
}

TEST(UILayerTest, NamesAreStable) {
    EXPECT_EQ(UILayerName(UILayerId::Hud), "Hud");
    EXPECT_EQ(UILayerName(UILayerId::Prompts), "Prompts");
    EXPECT_EQ(UILayerName(UILayerId::Panels), "Panels");
    EXPECT_EQ(UILayerName(UILayerId::Popups), "Popups");
}

TEST(UILayerTest, TradePromptLayerIsBelowPanels) {
    // Documents the known z-order gap: trade accept prompts render under open panels.
    EXPECT_LT(UILayerPriority(UILayerId::Prompts), UILayerPriority(UILayerId::Panels));
}
