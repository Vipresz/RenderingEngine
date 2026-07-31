#ifdef PLATFORM_HAS_NUKLEAR

#include <gtest/gtest.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_FONT_BAKING
#include "nuklear.h"

#include "ui/FontLibrary.h"

using Engine::Platform::UI::FontLibrary;
using Engine::Platform::UI::FontRole;

TEST(FontLibraryTest, RoleNamesAreStable) {
    EXPECT_STREQ(FontLibrary::RoleName(FontRole::UiRegular), "UiRegular");
    EXPECT_STREQ(FontLibrary::RoleName(FontRole::Hud), "Hud");
    EXPECT_STREQ(FontLibrary::RoleName(FontRole::Chat), "Chat");
}

TEST(FontLibraryTest, ResolvePathJoinsFontsRoot) {
    FontLibrary library;
    FontLibrary::Config config;
    config.fontsRoot = "menu/fonts";
    library.SetConfig(config);

    EXPECT_EQ(library.GetConfig().fontsRoot, "menu/fonts");
    EXPECT_FALSE(library.IsLoaded(FontRole::UiRegular));
    EXPECT_FALSE(library.HasAnyCustomFont());
}

TEST(FontLibraryTest, SetBakedFontTracksLoadedRoles) {
    FontLibrary library;
    nk_font dummyFont {};
    library.SetBakedFont(FontRole::Chat, &dummyFont);
    EXPECT_TRUE(library.IsLoaded(FontRole::Chat));
    EXPECT_TRUE(library.HasAnyCustomFont());
    EXPECT_EQ(library.GetFont(FontRole::Chat), &dummyFont);
}

#endif // PLATFORM_HAS_NUKLEAR
