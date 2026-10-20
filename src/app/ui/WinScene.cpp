#include "app/ui/WinScene.hpp"

#include <string>

namespace app::ui {

WinScene::WinScene(const sf::Font &font, const float width, const float height)
    : overlay_(createModalOverlay({width, height})),
      box_(createPanel(theme::sizes::kWinModal, PanelVariant::Modal)),
      title_("2048!", font, theme::typography::kHeadingSize),
      subtitle_(localizedText(font, "Kazandın", "Kazandin"), font,
                theme::typography::kSubtitleSize),
      scoreText_("", font, theme::typography::kWinScoreSize),
      continueText_("Devam Et", font, theme::typography::kButtonSize),
      continueButton_(createButtonForText(continueText_, ButtonVariant::Primary)),
      newGameText_("Yeni Oyun", font, theme::typography::kButtonSize),
      newGameButton_(createButtonForText(newGameText_, ButtonVariant::Secondary)) {
    box_.setOrigin(box_.getSize() / 2.f);
    box_.setPosition(width / 2.f, height / 2.f);
    styleText(title_, theme::colors::kTextPrimary, theme::typography::kHeadingStyle);
    styleText(subtitle_, theme::colors::kTextSecondary);
    styleText(scoreText_, theme::colors::kTextPrimary, theme::typography::kScoreValueStyle);
    styleButton(continueButton_, continueText_, ButtonVariant::Primary);
    styleButton(newGameButton_, newGameText_, ButtonVariant::Secondary);
    layout();
}

SceneCommand WinScene::handleEvent(const sf::Event &event, const sf::RenderWindow &window) {
    const bool continueActivated = continueFeedback_.handleEvent(event, window, continueButton_,
                                                                 focus_ == FocusTarget::Continue);
    const bool newGameActivated =
        newGameFeedback_.handleEvent(event, window, newGameButton_, focus_ == FocusTarget::NewGame);

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
        } else if (event.key.code == sf::Keyboard::Escape) {
            return activateControl(FocusTarget::Continue);
        } else if ((event.key.code == sf::Keyboard::Enter ||
                    event.key.code == sf::Keyboard::Space) &&
                   focus_ != FocusTarget::None) {
            pendingActivation_ = event.key.code;
        }
    }
    if (isPrimaryMouseRelease(event)) {
        resetInputState();
        if (continueActivated) {
            return activateControl(FocusTarget::Continue);
        }
        if (newGameActivated) {
            return activateControl(FocusTarget::NewGame);
        }
    }
    return SceneCommand::None;
}

void WinScene::onEnter() {
    panelReveal_.restart();
}

void WinScene::resetInputState() {
    continueFeedback_.reset();
    newGameFeedback_.reset();

    setFocus(FocusTarget::None);
}

void WinScene::setFocus(const FocusTarget focus) {
    focus_ = focus;
    pendingActivation_.reset();
    updateButtonFocus(continueButton_, focus_ == FocusTarget::Continue, continueFeedback_);
    updateButtonFocus(newGameButton_, focus_ == FocusTarget::NewGame, newGameFeedback_);
    updateButtonHover(continueButton_, focus_ == FocusTarget::Continue, ButtonVariant::Primary,
                      continueFeedback_);
    updateButtonHover(newGameButton_, focus_ == FocusTarget::NewGame, ButtonVariant::Secondary,
                      newGameFeedback_);
}

void WinScene::moveFocus(const bool backwards) {
    if (focus_ == FocusTarget::None) {
        setFocus(backwards ? FocusTarget::NewGame : FocusTarget::Continue);
    } else {
        setFocus(focus_ == FocusTarget::Continue ? FocusTarget::NewGame : FocusTarget::Continue);
    }
}

SceneCommand WinScene::activateControl(const FocusTarget target) {
    resetInputState();
    switch (target) {
    case FocusTarget::Continue:
        return SceneCommand::ContinueGame;
    case FocusTarget::NewGame:
        return SceneCommand::RestartGame;
    case FocusTarget::None:
        return SceneCommand::None;
    }
    return SceneCommand::None;
}

void WinScene::updateHover(const sf::Vector2f &mousePos) {
    updateButtonHover(
        continueButton_,
        focus_ == FocusTarget::Continue ||
            controlBounds(continueButton_, theme::sizes::kButtonBorder).contains(mousePos),
        ButtonVariant::Primary, continueFeedback_);
    updateButtonHover(
        newGameButton_,
        focus_ == FocusTarget::NewGame ||
            controlBounds(newGameButton_, theme::sizes::kButtonBorder).contains(mousePos),
        ButtonVariant::Secondary, newGameFeedback_);
}

void WinScene::render(sf::RenderTarget &target, const int score) {
    scoreText_.setString("Skor: " + std::to_string(score));
    fitTextWidth(scoreText_, box_.getSize().x - 2.f * theme::spacing::kXxl,
                 theme::typography::minimum::kWinScore);
    centerTextOrigin(scoreText_);
    panelReveal_.draw(target, [&](sf::RenderTarget &panelTarget) {
        drawModal(panelTarget, overlay_, box_);
        drawTrophyIcon(panelTarget, trophyPosition_, theme::sizes::kHeadingIconSize,
                       theme::colors::kTrophy);
        panelTarget.draw(title_);
        panelTarget.draw(subtitle_);
        panelTarget.draw(scoreText_);
        drawButton(panelTarget, continueButton_, continueText_, continueFeedback_);
        drawButton(panelTarget, newGameButton_, newGameText_, newGameFeedback_);
    });
}

void WinScene::layout() {
    const auto center = box_.getPosition();
    const auto size = box_.getSize();
    const float padding = theme::spacing::kXxl;
    const float top = center.y - size.y / 2.f + padding;
    const float iconSize = theme::sizes::kHeadingIconSize;
    trophyPosition_ = {center.x - iconSize / 2.f, top};

    centerTextOrigin(title_);
    title_.setPosition(center.x, top + iconSize + theme::spacing::kLg +
                                     theme::typography::kHeadingSize / 2.f);
    centerTextOrigin(subtitle_);
    subtitle_.setPosition(center.x, title_.getPosition().y + theme::typography::kHeadingSize / 2.f +
                                        theme::spacing::kSm +
                                        theme::typography::kSubtitleSize / 2.f);
    scoreText_.setPosition(center.x,
                           subtitle_.getPosition().y + theme::typography::kSubtitleSize / 2.f +
                               theme::spacing::kLg + theme::typography::kWinScoreSize / 2.f);

    const sf::Vector2f buttonSize{size.x - 2.f * padding, theme::sizes::kFormButtonHeight};
    continueButton_.setSize(buttonSize);
    continueButton_.setOrigin(buttonSize / 2.f);
    newGameButton_.setSize(buttonSize);
    newGameButton_.setOrigin(buttonSize / 2.f);
    newGameButton_.setPosition(center.x, center.y + size.y / 2.f - padding - buttonSize.y / 2.f);
    continueButton_.setPosition(center.x, newGameButton_.getPosition().y - buttonSize.y -
                                              theme::spacing::kMd);
    centerTextOrigin(continueText_);
    continueText_.setPosition(continueButton_.getPosition());
    centerTextOrigin(newGameText_);
    newGameText_.setPosition(newGameButton_.getPosition());
}

std::vector<AccessibilityNode> WinScene::accessibilityNodes(const int score) const {
    auto value =
        accessibleNode("score", AccessibilityRole::StaticText, "Skor",
                       {box_.getPosition().x - box_.getSize().x / 2.f + theme::spacing::kXxl,
                        scoreText_.getPosition().y - theme::typography::kWinScoreSize / 2.f,
                        box_.getSize().x - 2.f * theme::spacing::kXxl,
                        static_cast<float>(theme::typography::kWinScoreSize)});
    value.value = std::to_string(score);
    return {accessibleNode("title", AccessibilityRole::StaticText, "2048, Kazandın",
                           title_.getGlobalBounds()),
            value,
            accessibleButton("continue", "Devam Et",
                             controlBounds(continueButton_, theme::sizes::kButtonBorder),
                             focus_ == FocusTarget::Continue),
            accessibleButton("new", "Yeni Oyun",
                             controlBounds(newGameButton_, theme::sizes::kButtonBorder),
                             focus_ == FocusTarget::NewGame)};
}

bool WinScene::focusAccessible(const std::string_view id) {
    if (id == "continue")
        setFocus(FocusTarget::Continue);
    else if (id == "new")
        setFocus(FocusTarget::NewGame);
    else
        return false;
    return true;
}

SceneCommand WinScene::activateAccessible(const std::string_view id) {
    if (id == "continue")
        return activateControl(FocusTarget::Continue);
    if (id == "new")
        return activateControl(FocusTarget::NewGame);
    return SceneCommand::None;
}

} // namespace app::ui
