#pragma once

#include "app/ui/Scene.hpp"
#include "app/ui/SceneAccessibility.hpp"
#include "app/ui/UiPrimitives.hpp"

namespace app::ui {

class GameOverScene {
  public:
    GameOverScene(const sf::Font &font, const float width, const float height);

    SceneCommand handleEvent(const sf::Event &event, const sf::RenderWindow &window);

    void onEnter();

    void resetInputState();

    void updateHover(const sf::Vector2f &mousePos);

    void render(sf::RenderTarget &window, const int score, const int bestScore);

    std::vector<AccessibilityNode> accessibilityNodes(int score, int bestScore) const;
    bool focusAccessible(std::string_view id);
    SceneCommand activateAccessible(std::string_view id);

  private:
    PanelReveal panelReveal_;

    enum class FocusTarget { None, Replay, MainMenu };

    void layout();

    void setFocus(FocusTarget focus);

    void moveFocus(bool backwards);

    SceneCommand activateControl(FocusTarget target);

    sf::RectangleShape overlay_;
    RoundedRectShape box_;
    sf::Text title_;
    sf::Text newGameText_;
    RoundedRectShape newGameButton_;
    ButtonFeedback newGameFeedback_{ButtonVariant::Primary};
    sf::Text mainMenuText_;
    RoundedRectShape mainMenuButton_;
    ButtonFeedback mainMenuFeedback_{ButtonVariant::Secondary};
    float width_;
    float height_;
    FocusTarget focus_{FocusTarget::None};
    std::optional<sf::Keyboard::Key> pendingActivation_;
};

} // namespace app::ui
