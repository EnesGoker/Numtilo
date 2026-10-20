#pragma once

#include "app/ui/Scene.hpp"
#include "app/ui/SceneAccessibility.hpp"
#include "app/ui/UiPrimitives.hpp"
#include "core/Game.hpp"

#include <set>
#include <vector>

namespace app::ui {

class PlayingScene {
  public:
    explicit PlayingScene(const sf::Font &font);

    SceneAction handleEvent(const sf::Event &event, const sf::RenderWindow &window,
                            const float width);

    void setSoundEnabled(const bool enabled);

    void updateHover(const sf::Vector2f &mousePos, const float width);

    void tickVisuals();

    bool hasActiveAnimations() const;

    void resetVisualEffects();

    void onEnter();

    void resetInputState();

    void render(sf::RenderTarget &window, const sf::Font &font, const numtilo_core::Game &game,
                const float width, const int bestScore, std::string_view playerName);

    void onMove(const numtilo_core::MoveResult &result);

    std::vector<AccessibilityNode> accessibilityNodes(const numtilo_core::Game &game, float width,
                                                      int bestScore, std::string_view playerName);
    bool focusAccessible(std::string_view id);
    SceneAction activateAccessible(std::string_view id);

  private:
    PanelReveal panelReveal_;

    enum class FocusTarget { None, NewGame, Sound, MainMenu };

    struct FloatingScoreEffect {
        int scoreDelta;
        Clock::time_point startedAt;
    };

    void layoutControls(const float width);

    void setFocus(FocusTarget focus);

    void moveFocus(bool backwards);

    SceneCommand activateControl(FocusTarget target);

    void updateMoveAnimationState(const Clock::time_point now);

    void updateFloatingScores(const Clock::time_point now);

    TextFitResult renderFloatingScores(sf::RenderTarget &window, const sf::Font &font,
                                       sf::FloatRect scoreCard);

    sf::Text title_;
    sf::Text description_;
    sf::Text playerLabel_;
    sf::Text playerName_;
    RoundedRectShape newGameButton_;
    ButtonFeedback newGameFeedback_{ButtonVariant::Primary};
    RoundedRectShape soundRow_;
    PointerActivation soundPointer_;
    RoundedRectShape mainMenuButton_;
    ButtonFeedback mainMenuFeedback_{ButtonVariant::Secondary};
    sf::Text newGameText_;
    sf::Text soundText_;
    sf::Text mainMenuText_;
    bool soundEnabled_{true};
    bool soundHovered_{false};
    bool boardAccessibleFocused_{false};
    FocusTarget focus_{FocusTarget::None};
    std::optional<sf::Keyboard::Key> pendingActivation_;

    bool moveAnimationActive_{false};
    Clock::time_point moveAnimationStart_{};
    std::vector<numtilo_core::TileMovement> movingTiles_;
    std::vector<numtilo_core::TileMerge> mergeCells_;
    std::set<numtilo_core::Cell> hiddenDuringSlide_;
    std::set<numtilo_core::Cell> hiddenDuringSpawn_;
    std::optional<numtilo_core::SpawnedTile> spawnedTile_;
    std::vector<FloatingScoreEffect> floatingScores_;
};

} // namespace app::ui
