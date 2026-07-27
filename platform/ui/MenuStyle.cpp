#include "ui/MenuStyle.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "nuklear.h"

namespace Engine {
namespace Platform {
namespace UI {

MenuColor MenuColor::Rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    return MenuColor{r, g, b, a};
}

static nk_color ToNk(MenuColor c) {
    return nk_rgba(c.r, c.g, c.b, c.a);
}

static void ApplyButtonStyle(struct nk_style_button& btn, const MenuStyle& style) {
    const auto& p = style.palette;
    const auto& m = style.metrics;
    btn.normal = nk_style_item_color(ToNk(p.buttonNormal));
    btn.hover = nk_style_item_color(ToNk(p.buttonHover));
    btn.active = nk_style_item_color(ToNk(p.buttonActive));
    btn.border_color = ToNk(p.windowBorder);
    btn.text_background = ToNk(p.buttonNormal);
    btn.text_normal = ToNk(p.buttonText);
    btn.text_hover = ToNk(p.buttonText);
    btn.text_active = ToNk(p.buttonText);
    btn.padding = nk_vec2(m.buttonPaddingX, m.buttonPaddingY);
    btn.border = m.buttonBorder;
    btn.rounding = m.buttonRounding;
}

MenuStyle MenuStyle::DefaultDark() {
    return MenuStyle{};
}

MenuStyle MenuStyle::Astral() {
    MenuStyle s;
    s.palette.windowBackground = MenuColor::Rgba(18, 20, 28, 235);
    s.palette.buttonNormal = MenuColor::Rgba(40, 48, 68, 255);
    s.palette.buttonHover = MenuColor::Rgba(55, 75, 120, 255);
    s.palette.buttonActive = MenuColor::Rgba(75, 110, 170, 255);
    s.palette.titleText = MenuColor::Rgba(180, 200, 255, 255);
    s.metrics.menuButtonHeight = 48.0f;
    s.metrics.buttonRounding = 8.0f;
    s.metrics.windowRounding = 10.0f;
    s.metrics.fontSize = 18.0f;
    s.metrics.monoFontSize = 15.0f;
    s.metrics.hudFontSize = 36.0f;
    s.fontsRoot = "menus/main_menu/fonts";
    return s;
}

void MenuStyle::Apply(struct nk_context* ctx) const {
    if (!ctx) return;

    struct nk_style* style = &ctx->style;
    const auto& p = palette;
    const auto& m = metrics;

    style->window.padding = nk_vec2(m.windowPaddingX, m.windowPaddingY);
    style->window.spacing = nk_vec2(m.itemSpacing, m.itemSpacing);
    style->window.background = ToNk(p.windowBackground);
    style->window.border_color = ToNk(p.windowBorder);
    style->window.rounding = m.windowRounding;
    style->window.border = m.windowBorder;
    style->window.header.normal = nk_style_item_color(ToNk(p.windowBackground));
    style->window.header.hover = nk_style_item_color(ToNk(p.buttonHover));
    style->window.header.active = nk_style_item_color(ToNk(p.buttonActive));
    style->window.header.label_normal = ToNk(p.titleText);
    style->window.header.label_hover = ToNk(p.titleText);
    style->window.header.label_active = ToNk(p.titleText);

    ApplyButtonStyle(style->button, *this);
    ApplyButtonStyle(style->contextual_button, *this);
    ApplyButtonStyle(style->menu_button, *this);

    style->text.color = ToNk(p.bodyText);

    style->scrollh.normal = nk_style_item_color(ToNk(p.scrollbar));
    style->scrollh.hover = nk_style_item_color(ToNk(p.buttonHover));
    style->scrollh.active = nk_style_item_color(ToNk(p.buttonActive));
    style->scrollh.border_color = ToNk(p.windowBorder);
    style->scrollh.cursor_normal = nk_style_item_color(ToNk(p.buttonNormal));
    style->scrollh.cursor_hover = nk_style_item_color(ToNk(p.buttonHover));
    style->scrollh.cursor_active = nk_style_item_color(ToNk(p.buttonActive));

    style->scrollv = style->scrollh;
}

} // namespace UI
} // namespace Platform
} // namespace Engine
