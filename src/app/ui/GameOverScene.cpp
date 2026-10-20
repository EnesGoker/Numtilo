#include "app/ui/GameOverScene.hpp"

#include <string>

namespace app::ui {

GameOverScene::GameOverScene(const sf::Font &font, const float width, const float height)
    : overlay_(createModalOverlay({width, height})),
      box_(createPanel(theme::sizes::kGameOverModal, PanelVariant::Modal)),
      title_("Oyun Bitti", font, theme::typography::kHeadingSize),
      newGameText_("Tekrar Oyna", font, theme::typography::kButtonSize),
      newGameButton_(createButtonForText(newGameText_, ButtonVariant::Primary)),
      mainMenuText_(localizedText(font, "Ana Menü", "Ana Menu"), font,
                    theme::typography::kButtonSize),
      mainMenuButton_(createButtonForText(mainMenuText_, ButtonVariant::Secondary)), width_(width),
      height_(height) {
    box_.setOrigin(box_.getSize() / 2.f);

    styleText(title_, theme::colors::kTextPrimary, theme::typography::kHeadingStyle);
    centerTextOrigin(title_);

    styleButton(newGameButton_, newGameText_, ButtonVariant::Primary);
    centerTextOrigin(newGameText_);

    styleButton(mainMenuButton_, mainMenuText_, ButtonVariant::Secondary);
    centerTextOrigin(mainMenuText_);

    layout();
}

SceneCommand GameOverScene::handleEvent(const sf::Event &event, const sf::RenderWindow &window) {
    const bool replayActivated =
        newGameFeedback_.handleEvent(event, window, newGameButton_, focus_ == FocusTarget::Replay);
    const bool mainMenuActivated = mainMenuFeedback_.handleEvent(event, window, mainMenuButton_,
                                                                 focus_ == FocusTarget::MainMenu);

    if (isPrimaryMousePress(event)) {
        pendingActivation_.reset();
    }

    if (event.type == sf::Event::LostFocus) {
        resetInputState();
        return SceneCommand::None;
    }

    if (event.type == sf::Event::KeyReleased) {
        if (pendingActivation_ == event.key.code) {
            pendingActivation_.reset();
            return activateControl(focus_);
        }
        return SceneCommand::None;
    }

    if (event.type == sf::Event::KeyPressed) {
        if (event.key.alt || event.key.control || event.key.system) {
            return SceneCommand::None;
        }
        if (event.key.code == sf::Keyboard::Tab) {
            moveFocus(event.key.shift);
            return SceneCommand::None;
        }
        if (event.key.code == sf::Keyboard::N) {
            return activateControl(FocusTarget::Replay);
        }
        if (event.key.code == sf::Keyboard::Escape) {
            return activateControl(FocusTarget::MainMenu);
        }
        if (event.key.code == sf::Keyboard::Enter || event.key.code == sf::Keyboard::Space) {
            if (focus_ != FocusTarget::None) {
                pendingActivation_ = event.key.code;
                return SceneCommand::None;
            }
            if (event.key.code == sf::Keyboard::Enter) {
                return activateControl(FocusTarget::Replay);
            }
        }
    }

    if (isPrimaryMouseRelease(event)) {
        resetInputState();
        if (replayActivated) {
            return activateControl(FocusTarget::Replay);
        }
        if (mainMenuActivated) {
            return activateControl(FocusTarget::MainMenu);
        }
    }

    return SceneCommand::None;
}

void GameOverScene::onEnter() {
    panelReveal_.restart();
}

void GameOverScene::resetInputState() {
    newGameFeedback_.reset();
    mainMenuFeedback_.reset();

    setFocus(FocusTarget::None);
}

void GameOverScene::setFocus(const FocusTarget focus) {
    focus_ = focus;
    pendingActivation_.reset();
    updateButtonFocus(newGameButton_, focus_ == FocusTarget::Replay, newGameFeedback_);
    updateButtonFocus(mainMenuButton_, focus_ == FocusTarget::MainMenu, mainMenuFeedback_);
    updateButtonHover(newGameButton_, focus_ == FocusTarget::Replay, ButtonVariant::Primary,
                      newGameFeedback_);
    updateButtonHover(mainMenuButton_, focus_ == FocusTarget::MainMenu, ButtonVariant::Secondary,
                      mainMenuFeedback_);
}

void GameOverScene::moveFocus(const bool backwards) {
    if (focus_ == FocusTarget::None) {
        setFocus(backwards ? FocusTarget::MainMenu : FocusTarget::Replay);
    } else {
        setFocus(focus_ == FocusTarget::Replay ? FocusTarget::MainMenu : FocusTarget::Replay);
    }
}

SceneCommand GameOverScene::activateControl(const FocusTarget target) {
    resetInputState();
    switch (target) {
    case FocusTarget::Replay:
        return SceneCommand::RestartGame;
    case FocusTarget::MainMenu:
        return SceneCommand::ShowSplash;
    case FocusTarget::None:
        return SceneCommand::None;
    }
    return SceneCommand::None;
}

void GameOverScene::updateHover(const sf::Vector2f &mousePos) {
    updateButtonHover(
        newGameButton_,
        focus_ == FocusTarget::Replay ||
            controlBounds(newGameButton_, theme::sizes::kButtonBorder).contains(mousePos),
        ButtonVariant::Primary, newGameFeedback_);
    updateButtonHover(
        mainMenuButton_,
        focus_ == FocusTarget::MainMenu ||
            controlBounds(mainMenuButton_, theme::sizes::kButtonBorder).contains(mousePos),
        ButtonVariant::Secondary, mainMenuFeedback_);
}

void GameOverScene::render(sf::RenderTarget &window, const int score, const int bestScore) {
    panelReveal_.draw(window, [&](sf::RenderTarget &panelTarget) {
        // Dim the full client surface, including the content view's outer margins.
        // Restore the caller's view before drawing the card or mapping later input.
        const auto contentView = panelTarget.getView();
        const auto &fullView = panelTarget.getDefaultView();
        overlay_.setSize(fullView.getSize());
        overlay_.setPosition(fullView.getCenter() - fullView.getSize() / 2.f);
        panelTarget.setView(fullView);
        panelTarget.draw(overlay_);
        panelTarget.setView(contentView);
        drawPanel(panelTarget, box_);

        const auto panelBounds = box_.getGlobalBounds();
        const float panelTop = panelBounds.top + box_.getOutlineThickness();
        title_.setPosition(width_ / 2.f,
                           panelTop + theme::spacing::kXxl + theme::typography::kHeadingSize / 2.f);
        panelTarget.draw(title_);

        const float contentTop =
            title_.getGlobalBounds().top + title_.getGlobalBounds().height + theme::spacing::kXl;
        // Focus changes the outline, not the score-card layout above the button face.
        const float contentBottom = newGameButton_.getPosition().y -
                                    newGameButton_.getSize().y / 2.f - theme::sizes::kButtonBorder -
                                    theme::spacing::kXl;
        const float cardHeight = theme::sizes::kGameOverScoreCardHeight;
        const float cardTop = contentTop + (contentBottom - contentTop - cardHeight) / 2.f;
        const float gap = theme::spacing::kLg;
        const float cardWidth = (box_.getSize().x - 2.f * theme::spacing::kXxl - gap) / 2.f;
        const float cardLeft = box_.getPosition().x - box_.getSize().x / 2.f + theme::spacing::kXxl;
        const auto &font = *title_.getFont();
        drawScoreCard(panelTarget, font, {cardLeft, cardTop, cardWidth, cardHeight}, "Son Skor",
                      sf::String(std::to_string(score)));
        drawScoreCard(
            panelTarget, font, {cardLeft + cardWidth + gap, cardTop, cardWidth, cardHeight},
            localizedText(font, "En İyi", "En Iyi"), sf::String(std::to_string(bestScore)));

        drawButton(panelTarget, newGameButton_, newGameText_, ButtonIcon::Restart,
                   newGameFeedback_);
        drawButton(panelTarget, mainMenuButton_, mainMenuText_, ButtonIcon::Home,
                   mainMenuFeedback_);
    });
}

void GameOverScene::layout() {
    box_.setPosition(width_ / 2.f, height_ / 2.f);
    const float padding = theme::spacing::kXxl;
    const sf::Vector2f buttonSize{box_.getSize().x - 2.f * padding,
                                  theme::sizes::kFormButtonHeight};
    newGameButton_.setSize(buttonSize);
    mainMenuButton_.setSize(buttonSize);
    newGameButton_.setOrigin(buttonSize / 2.f);
    mainMenuButton_.setOrigin(buttonSize / 2.f);
    mainMenuButton_.setPosition(width_ / 2.f, box_.getPosition().y + box_.getSize().y / 2.f -
                                                  padding - buttonSize.y / 2.f);
    newGameButton_.setPosition(width_ / 2.f, mainMenuButton_.getPosition().y - buttonSize.y -
                                                 theme::spacing::kMd);
    newGameText_.setPosition(newGameButton_.getPosition());
    mainMenuText_.setPosition(mainMenuButton_.getPosition());
}

std::vector<AccessibilityNode> GameOverScene::accessibilityNodes(const int score,
                                                                 const int bestScore) const {
    const float contentTop =
        title_.getGlobalBounds().top + title_.getGlobalBounds().height + theme::spacing::kXl;
    const float contentBottom = newGameButton_.getPosition().y - newGameButton_.getSize().y / 2.f -
                                theme::sizes::kButtonBorder - theme::spacing::kXl;
    const float cardHeight = theme::sizes::kGameOverScoreCardHeight;
    const float cardTop = contentTop + (contentBottom - contentTop - cardHeight) / 2.f;
    const float cardWidth =
        (box_.getSize().x - 2.f * theme::spacing::kXxl - theme::spacing::kLg) / 2.f;
    const float cardLeft = box_.getPosition().x - box_.getSize().x / 2.f + theme::spacing::kXxl;
    auto result = accessibleNode("score", AccessibilityRole::StaticText, "Son Skor",
                                 {cardLeft, cardTop, cardWidth, cardHeight});
    result.value = std::to_string(score);
    auto best = accessibleNode(
        "best", AccessibilityRole::StaticText, "En İyi",
        {cardLeft + cardWidth + theme::spacing::kLg, cardTop, cardWidth, cardHeight});
    best.value = std::to_string(bestScore);
    return {accessibleNode("title", AccessibilityRole::StaticText, "Oyun Bitti",
                           title_.getGlobalBounds()),
            result, best,
            accessibleButton("replay", "Tekrar Oyna",
                             controlBounds(newGameButton_, theme::sizes::kButtonBorder),
                             focus_ == FocusTarget::Replay),
            accessibleButton("menu", "Ana Menü",
                             controlBounds(mainMenuButton_, theme::sizes::kButtonBorder),
                             focus_ == FocusTarget::MainMenu)};
}

bool GameOverScene::focusAccessible(const std::string_view id) {
    if (id == "replay")
        setFocus(FocusTarget::Replay);
    else if (id == "menu")
        setFocus(FocusTarget::MainMenu);
    else
        return false;
    return true;
}

SceneCommand GameOverScene::activateAccessible(const std::string_view id) {
    if (id == "replay")
        return activateControl(FocusTarget::Replay);
    if (id == "menu")
        return activateControl(FocusTarget::MainMenu);
    return SceneCommand::None;
}

} // namespace app::ui
