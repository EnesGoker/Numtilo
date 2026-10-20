#pragma once

#include "app/ui/Scene.hpp"
#include "app/ui/SceneAccessibility.hpp"
#include "app/ui/UiPrimitives.hpp"

#include <array>
#include <cstddef>
#include <string>

namespace app::ui {

class SplashScene {
  public:
    SplashScene(const sf::Font &font, const float width, const float height);

    const std::string &playerName() const noexcept;

    SceneCommand handleEvent(const sf::Event &event, const sf::RenderWindow &window);

    void onEnter();

    void resetInputState();

    void updateHover(const sf::Vector2f &mousePos);

    void render(sf::RenderTarget &window) const;

    std::vector<AccessibilityNode> accessibilityNodes() const;
    bool focusAccessible(std::string_view id);
    bool setAccessiblePlayerName(std::string_view value);
    SceneCommand activateAccessible(std::string_view id);

  private:
    mutable PanelReveal panelReveal_;

    enum class FocusTarget { None, Name, Start, Scores };
    enum class NameIssue { Empty, Unsupported, TooWide };

    struct Decoration {
        RoundedRectShape tile;
        sf::Text label;
    };

    static constexpr std::size_t kMaxNameLength = 18U;

    static Decoration createDecoration(const sf::Font &font, int value, float size,
                                       sf::Vector2f center, float rotation);

    static bool isWhitespace(const sf::Uint32 codePoint);

    static sf::String trimInput(const sf::String &value);

    static std::string toUtf8(const sf::String &value);

    void refreshNameText();

    void setNameInvalid(bool invalid, NameIssue issue = NameIssue::Empty);

    void setFocus(FocusTarget focus);

    void moveFocus(bool backwards);

    SceneCommand validateAndStart();

    sf::Text title_;
    sf::Text subtitle_;
    sf::Text nameLabel_;
    sf::Text nameText_;
    sf::Text startText_;
    RoundedRectShape startButton_;
    ButtonFeedback startFeedback_{ButtonVariant::Primary};
    sf::Text scoresText_;
    RoundedRectShape scoresButton_;
    ButtonFeedback scoresFeedback_{ButtonVariant::Secondary};
    sf::Text validationText_;
    RoundedRectShape nameBox_;
    PointerActivation namePointer_;
    RoundedRectShape formPanel_;
    std::array<Decoration, 4> decorations_;

    FocusTarget focus_{FocusTarget::None};
    std::optional<sf::Keyboard::Key> pendingActivation_;
    bool nameInvalid_{false};
    bool nameLimitReached_{false};
    sf::String nameInput_;
    std::string playerName_;
};

} // namespace app::ui
