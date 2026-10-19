#ifdef PLATFORM_HAS_NUKLEAR

#include <gtest/gtest.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "nuklear.h"

#include "ui/ItemTooltip.h"

#include <cstring>

using Engine::Platform::UI::ItemTooltipContent;
using Engine::Platform::UI::ItemTooltipStyle;
using Engine::Platform::UI::LayoutItemActionPanel;
using Engine::Platform::UI::MeasureItemTooltip;

namespace {

float MockTextWidth(nk_handle /*handle*/, float /*height*/, const char* text, int len) {
    return static_cast<float>(len > 0 ? len : 0) * 7.0f;
}

nk_user_font MakeMockFont(float height = 16.0f) {
    nk_user_font font {};
    font.userdata = nk_handle_ptr(nullptr);
    font.height = height;
    font.width = MockTextWidth;
    return font;
}

} // namespace

TEST(ItemTooltipLayoutTest, MeasureEmptyContentUsesMinimumSize) {
    const nk_user_font font = MakeMockFont();
    const struct nk_rect tooltipBounds = MeasureItemTooltip({}, &font);
    EXPECT_GE(tooltipBounds.w, 0.0f);
    EXPECT_GE(tooltipBounds.h, 0.0f);
}

TEST(ItemTooltipLayoutTest, MeasureIncludesPaddingAndDescriptionWrap) {
    ItemTooltipContent content;
    content.name = "Iron Sword";
    content.typeLabel = "Weapon";
    content.description = "A sturdy blade forged for testing tooltip layout.";

    const nk_user_font font = MakeMockFont(16.0f);
    ItemTooltipStyle style;
    style.maxWidth = 120.0f;
    style.padding = 10.0f;

    const struct nk_rect tooltipBounds = MeasureItemTooltip(content, &font, style);
    EXPECT_GT(tooltipBounds.w, style.padding * 2.0f);
    EXPECT_GT(tooltipBounds.h, style.padding * 2.0f + font.height);
    EXPECT_LE(tooltipBounds.w, style.maxWidth);
}

TEST(ItemTooltipLayoutTest, ActionPanelPlacesButtonsBelowTooltip) {
    ItemTooltipContent content;
    content.name = "Potion";
    content.typeLabel = "Consumable";

    const nk_user_font font = MakeMockFont(16.0f);
    const struct nk_rect anchorRect{100.0f, 100.0f, 32.0f, 32.0f};
    const auto layout = LayoutItemActionPanel(anchorRect,
                                              800,
                                              600,
                                              content,
                                              true,
                                              true,
                                              false,
                                              &font);
    EXPECT_GT(layout.panelBounds.h, 0.0f);
    EXPECT_TRUE(layout.hasUseButton);
    EXPECT_TRUE(layout.hasTradeButton);
    EXPECT_FALSE(layout.hasTradeXButton);
    EXPECT_GT(layout.useButtonBounds.h, 0.0f);
    EXPECT_GT(layout.tradeButtonBounds.h, 0.0f);
    EXPECT_GE(layout.useButtonBounds.y, layout.panelBounds.y);
}

#endif // PLATFORM_HAS_NUKLEAR
