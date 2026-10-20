#pragma once

#include "app/ui/Scene.hpp"
#include "app/ui/SceneAccessibility.hpp"
#include "app/ui/UiPrimitives.hpp"
#include "core/ScoreManager.hpp"

#include <vector>

namespace app::ui {

class HighScoresScene {
  public:
    HighScoresScene(const sf::Font &font, const float width, const float height);

    SceneCommand handleEvent(const sf::Event &event, const sf::RenderWindow &window);

    void onEnter();

    void resetInputState();

    void updateHover(const sf::Vector2f &mousePos);

    void render(sf::RenderTarget &window, const std::vector<numtilo_core::ScoreEntry> &entries);

    std::vector<AccessibilityNode>
    accessibilityNodes(const std::vector<numtilo_core::ScoreEntry> &entries) const;
    bool focusAccessible(std::string_view id);
    SceneCommand activateAccessible(std::string_view id);

  private:
    PanelReveal panelReveal_;

    void setBackFocused(bool focused);

    const sf::Font &font_;
    sf::Text title_;
    sf::Text description_;
    sf::FloatRect trophyBounds_;
    sf::String nameHeader_;
    sf::String scoreHeader_;
    sf::Text emptyText_;
    sf::Text backText_;
    RoundedRectShape backButton_;
    ButtonFeedback backFeedback_{ButtonVariant::Secondary};
    RoundedRectShape tableBox_;
    float width_;
    float height_;
    bool backFocused_{false};
    std::optional<sf::Keyboard::Key> pendingActivation_;
};

} // namespace app::ui
