#pragma once

#include "app/ui/Scene.hpp"
#include "app/ui/SceneAccessibility.hpp"
#include "app/ui/UiPrimitives.hpp"

namespace app::ui {

// Modal input produces commands; App owns round transitions and persistence.
class WinScene {
  public:
    WinScene(const sf::Font &font, float width, float height);

    SceneCommand handleEvent(const sf::Event &event, const sf::RenderWindow &window);

    void onEnter();

    void resetInputState();

    void updateHover(const sf::Vector2f &mousePos);

    void render(sf::RenderTarget &target, int score);

    std::vector<AccessibilityNode> accessibilityNodes(int score) const;
    bool focusAccessible(std::string_view id);
    SceneCommand activateAccessible(std::string_view id);

  private:
    PanelReveal panelReveal_;

    enum class FocusTarget { None, Continue, NewGame };

    void layout();

    void setFocus(FocusTarget focus);

    void moveFocus(bool backwards);

    SceneCommand activateControl(FocusTarget target);

    sf::RectangleShape overlay_;
    RoundedRectShape box_;
    sf::Text title_;
    sf::Text subtitle_;
    sf::Text scoreText_;
    sf::Text continueText_;
    RoundedRectShape continueButton_;
    ButtonFeedback continueFeedback_{ButtonVariant::Primary};
    sf::Text newGameText_;
    RoundedRectShape newGameButton_;
    ButtonFeedback newGameFeedback_{ButtonVariant::Secondary};
    sf::Vector2f trophyPosition_;
    FocusTarget focus_{FocusTarget::None};
    std::optional<sf::Keyboard::Key> pendingActivation_;
};

} // namespace app::ui
