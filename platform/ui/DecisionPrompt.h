#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

#include <functional>
#include <string>

struct nk_context;
struct nk_rect;

namespace Engine {
namespace Platform {
namespace UI {

// Top-center accept/decline (or custom two-button) prompt for in-game decisions.
struct DecisionPromptContent {
    std::string title;
    std::string message;
    std::string primaryLabel = "Accept";
    std::string secondaryLabel = "Decline";
    bool showPrimary = true;
    bool showSecondary = true;
};

class DecisionPrompt {
public:
    void SetVisible(bool visible) { m_visible = visible; }
    bool IsVisible() const { return m_visible; }
    bool BlocksGameInput() const { return m_visible; }

    void SetContent(DecisionPromptContent content);
    void SetPrimaryCallback(std::function<void()> callback);
    void SetSecondaryCallback(std::function<void()> callback);

    void Draw(struct nk_context* ctx, int screenWidth, int screenHeight);

private:
    bool ComputeLayout(struct nk_context* ctx,
                       int screenWidth,
                       int screenHeight,
                       struct nk_rect& outPanel,
                       struct nk_rect& outPrimary,
                       struct nk_rect& outSecondary,
                       float& outRowH) const;

    bool m_visible = false;
    DecisionPromptContent m_content;
    std::function<void()> m_primaryCallback;
    std::function<void()> m_secondaryCallback;
};

} // namespace UI
} // namespace Platform
} // namespace Engine

#endif // PLATFORM_HAS_NUKLEAR
