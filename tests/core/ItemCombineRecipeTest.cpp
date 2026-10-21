#include <gtest/gtest.h>

#include "core/network/ItemCombineRecipe.h"

using Engine::ECS::PlayerItemOwnershipComponent;
using Engine::Net::ApplyItemCombine;
using Engine::Net::FindCombineRecipe;
using Engine::Net::ItemCombineRecipe;

namespace {

ItemCombineRecipe HerbRecipe() {
    ItemCombineRecipe recipe;
    recipe.inputA = "herb";
    recipe.inputB = "vial";
    recipe.outputItemId = "potion";
    recipe.outputQuantity = 1;
    return recipe;
}

} // namespace

TEST(ItemCombineRecipeTest, FindsARecipeInEitherInputOrder) {
    const std::vector<ItemCombineRecipe> recipes{HerbRecipe()};
    const auto forward = FindCombineRecipe(recipes, "herb", "vial");
    const auto reverse = FindCombineRecipe(recipes, "vial", "herb");
    ASSERT_TRUE(forward.has_value());
    ASSERT_TRUE(reverse.has_value());
    EXPECT_EQ(forward->outputItemId, "potion");
    EXPECT_EQ(reverse->outputItemId, "potion");
    EXPECT_FALSE(FindCombineRecipe(recipes, "herb", "stone").has_value());
}

TEST(ItemCombineRecipeTest, CombineConsumesInputsAndGrantsTheOutput) {
    PlayerItemOwnershipComponent bag;
    ASSERT_TRUE(bag.GrantItem("herb", 2));
    ASSERT_TRUE(bag.GrantItem("vial", 1));

    EXPECT_TRUE(ApplyItemCombine(bag, 0, 1, HerbRecipe()));
    EXPECT_EQ(bag.CountItem("herb"), 1u);
    EXPECT_EQ(bag.CountItem("vial"), 0u);
    EXPECT_EQ(bag.CountItem("potion"), 1u);
}

TEST(ItemCombineRecipeTest, CombineRejectsTheSameSlotOrTheWrongItems) {
    PlayerItemOwnershipComponent bag;
    ASSERT_TRUE(bag.GrantItem("herb", 1));
    ASSERT_TRUE(bag.GrantItem("stone", 1));

    EXPECT_FALSE(ApplyItemCombine(bag, 0, 0, HerbRecipe()));
    EXPECT_FALSE(ApplyItemCombine(bag, 0, 1, HerbRecipe()));
    EXPECT_EQ(bag.CountItem("herb"), 1u);
    EXPECT_EQ(bag.CountItem("stone"), 1u);
    EXPECT_EQ(bag.CountItem("potion"), 0u);
}
