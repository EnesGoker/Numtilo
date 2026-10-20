#include "app/ui/SplashScene.hpp"
#include "app/ui/UiTheme.hpp"

namespace app::ui {

SplashScene::Decoration SplashScene::createDecoration(const sf::Font &font, const int value,
                                                      const float size, const sf::Vector2f center,
                                                      const float rotation) {
    Decoration decoration{RoundedRectShape({size, size}, theme::radius::kTile),
                          sf::Text(std::to_string(value), font,
                                   value < 100 ? theme::typography::kDecorationNumberSize
                                               : theme::typography::kDecorationLongNumberSize)};
    decoration.tile.setFillColor(getTileColor(value));
    decoration.tile.setOrigin(size / 2.f, size / 2.f);
    decoration.tile.setPosition(center);
    decoration.tile.setRotation(rotation);
    styleText(decoration.label, getTileTextColor(value), theme::typography::kEmphasisStyle);
    centerTextOrigin(decoration.label);
    decoration.label.setPosition(center);
    decoration.label.setRotation(rotation);
    return decoration;
}

SplashScene::SplashScene(const sf::Font &font, const float width, const float height)
    : title_("Numtilo", font, theme::typography::kDisplaySize),
      subtitle_(localizedText(font, "Sayıları birleştir", "Sayilari birlestir"), font,
                theme::typography::kSubtitleSize),
      nameLabel_(localizedText(font, "Oyuncu Adı", "Oyuncu Adi"), font,
                 theme::typography::kLabelSize),
      nameText_("", font, theme::typography::kInputSize),
      startText_(localizedText(font, "Oyuna Başla", "Oyuna Basla"), font,
                 theme::typography::kButtonSize),
      startButton_(createButtonForText(startText_, ButtonVariant::Primary)),
      scoresText_("Skorlar", font, theme::typography::kButtonSize),
      scoresButton_(createButtonForText(scoresText_, ButtonVariant::Secondary)),
      validationText_("", font, theme::typography::kCaptionSize), nameBox_(createTextInput()),
      formPanel_(createPanel({})), decorations_{{
                                       createDecoration(font, 2, theme::sizes::kDecorationSmall,
                                                        {width * 0.14f, height * 0.22f}, -16.f),
                                       createDecoration(font, 8, theme::sizes::kDecorationMedium,
                                                        {width * 0.86f, height * 0.25f}, 18.f),
                                       createDecoration(font, 2048, theme::sizes::kDecorationLarge,
                                                        {width * 0.13f, height * 0.60f}, -14.f),
                                       createDecoration(font, 4, theme::sizes::kDecorationSmall,
                                                        {width * 0.86f, height * 0.82f}, 14.f),
                                   }} {
    styleText(title_, theme::colors::kTextPrimary, theme::typography::kDisplayStyle);
    centerTextOrigin(title_);
    styleText(subtitle_, theme::colors::kTextSecondary);
    centerTextOrigin(subtitle_);
    styleText(nameLabel_, theme::colors::kTextSecondary);
    const auto labelBounds = nameLabel_.getLocalBounds();
    nameLabel_.setOrigin(labelBounds.left, labelBounds.top);
    nameBox_.setOrigin(nameBox_.getSize() / 2.f);
    updateTextInputStyle(nameBox_, focus_ == FocusTarget::Name);
    styleText(nameText_, theme::colors::kTextPrimary);
    styleText(validationText_, theme::colors::kError);
    centerTextOrigin(validationText_);

    styleButton(startButton_, startText_, ButtonVariant::Primary);
    styleButton(scoresButton_, scoresText_, ButtonVariant::Secondary);
    centerTextOrigin(startText_);
    centerTextOrigin(scoresText_);
    startButton_.setSize({nameBox_.getSize().x, theme::sizes::kFormButtonHeight});
    scoresButton_.setSize({nameBox_.getSize().x, theme::sizes::kFormButtonHeight});
    startButton_.setOrigin(startButton_.getSize() / 2.f);
    scoresButton_.setOrigin(scoresButton_.getSize() / 2.f);

    const float centerX = width / 2.f;
    const float titleHeight = title_.getLocalBounds().height;
    const float subtitleHeight = subtitle_.getLocalBounds().height;
    const float validationHeight = static_cast<float>(theme::typography::kCaptionSize);
    const float headerHeight = titleHeight + theme::spacing::kSm + subtitleHeight;
    const float formHeight = labelBounds.height + theme::spacing::kMd + nameBox_.getSize().y +
                             theme::spacing::kSm + validationHeight + theme::spacing::kLg +
                             startButton_.getSize().y + theme::spacing::kMd +
                             scoresButton_.getSize().y;
    const float formPadding = theme::spacing::kXl;
    formPanel_.setSize({nameBox_.getSize().x + 2.f * formPadding, formHeight + 2.f * formPadding});
    const float groupHeight = headerHeight + theme::spacing::kXl + formPanel_.getSize().y;
    float top = (height - groupHeight) / 2.f;
    title_.setPosition(centerX, top + titleHeight / 2.f);
    top += titleHeight + theme::spacing::kSm;
    subtitle_.setPosition(centerX, top + subtitleHeight / 2.f);
    top += subtitleHeight + theme::spacing::kXl;
    formPanel_.setPosition(centerX - formPanel_.getSize().x / 2.f, top);
    top += formPadding;
    nameLabel_.setPosition(centerX - nameBox_.getSize().x / 2.f, top);
    top += labelBounds.height + theme::spacing::kMd;
    nameBox_.setPosition(centerX, top + nameBox_.getSize().y / 2.f);
    top += nameBox_.getSize().y + theme::spacing::kSm;
    validationText_.setPosition(centerX, top + validationHeight / 2.f);
    top += validationHeight + theme::spacing::kLg;
    startButton_.setPosition(centerX, top + startButton_.getSize().y / 2.f);
    startText_.setPosition(startButton_.getPosition());
    top += startButton_.getSize().y + theme::spacing::kMd;
    scoresButton_.setPosition(centerX, top + scoresButton_.getSize().y / 2.f);
    scoresText_.setPosition(scoresButton_.getPosition());

    refreshNameText();
}

const std::string &SplashScene::playerName() const noexcept {
    return playerName_;
}

SceneCommand SplashScene::handleEvent(const sf::Event &event, const sf::RenderWindow &window) {
    const bool startActivated =
        startFeedback_.handleEvent(event, window, startButton_, focus_ == FocusTarget::Start);
    const bool scoresActivated =
        scoresFeedback_.handleEvent(event, window, scoresButton_, focus_ == FocusTarget::Scores);
    const bool nameActivated = namePointer_.handleEvent(event, [&](const sf::Vector2i &pixel) {
        return containsWithPadding(nameBox_.getGlobalBounds(), window.mapPixelToCoords(pixel));
    });

    if (isPrimaryMousePress(event)) {
        pendingActivation_.reset();
    }

    if (event.type == sf::Event::LostFocus) {
        setFocus(FocusTarget::None);
        return SceneCommand::None;
    }

    if (event.type == sf::Event::KeyReleased) {
        if (pendingActivation_ == event.key.code) {
            pendingActivation_.reset();
            if (focus_ == FocusTarget::Scores) {
                setNameInvalid(false);
                setFocus(FocusTarget::None);
                return SceneCommand::ShowHighScores;
            }
            if (focus_ == FocusTarget::Start) {
                return validateAndStart();
            }
        }
        return SceneCommand::None;
    }

    if (event.type == sf::Event::KeyPressed &&
        (event.key.alt || event.key.control || event.key.system)) {
        return SceneCommand::None;
    }

    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
        resetInputState();
        return SceneCommand::None;
    }

    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Tab) {
        moveFocus(event.key.shift);
        return SceneCommand::None;
    }

    if (event.type == sf::Event::KeyPressed &&
        (event.key.code == sf::Keyboard::Enter ||
         (event.key.code == sf::Keyboard::Space &&
          (focus_ == FocusTarget::Start || focus_ == FocusTarget::Scores)))) {
        if (focus_ == FocusTarget::Start || focus_ == FocusTarget::Scores) {
            pendingActivation_ = event.key.code;
            return SceneCommand::None;
        }
        return validateAndStart();
    }

    if (isPrimaryMouseRelease(event)) {
        if (nameActivated) {
            setFocus(FocusTarget::Name);
            return SceneCommand::None;
        }
        if (startActivated) {
            setFocus(FocusTarget::Start);
            return validateAndStart();
        }
        if (scoresActivated) {
            setNameInvalid(false);
            setFocus(FocusTarget::None);
            return SceneCommand::ShowHighScores;
        }
        setFocus(FocusTarget::None);
        return SceneCommand::None;
    }

    if (focus_ == FocusTarget::Name && event.type == sf::Event::TextEntered) {
        const auto unicode = event.text.unicode;
        if (unicode == 8U) {
            if (nameInput_.getSize() > 0U) {
                nameInput_.erase(nameInput_.getSize() - 1U, 1U);
                nameLimitReached_ = false;
                refreshNameText();
            }
        } else if (unicode >= 32U && unicode != 127U && unicode <= 0x10FFFFU &&
                   !(unicode >= 0xD800U && unicode <= 0xDFFFU)) {
            if (nameInput_.getSize() < kMaxNameLength) {
                nameInput_ += unicode;
                nameLimitReached_ = false;
                refreshNameText();
            } else {
                nameLimitReached_ = true;
                refreshNameText();
            }
        }
    }

    return SceneCommand::None;
}

void SplashScene::onEnter() {
    refreshNameText();
    panelReveal_.restart();
}

void SplashScene::resetInputState() {
    startFeedback_.reset();
    scoresFeedback_.reset();
    namePointer_.reset();

    setFocus(FocusTarget::None);
}

void SplashScene::updateHover(const sf::Vector2f &mousePos) {
    updateButtonHover(startButton_,
                      controlBounds(startButton_, theme::sizes::kButtonBorder).contains(mousePos) ||
                          focus_ == FocusTarget::Start,
                      ButtonVariant::Primary, startFeedback_);
    updateButtonHover(
        scoresButton_,
        controlBounds(scoresButton_, theme::sizes::kButtonBorder).contains(mousePos) ||
            focus_ == FocusTarget::Scores,
        ButtonVariant::Secondary, scoresFeedback_);
    updateTextInputStyle(nameBox_, focus_ == FocusTarget::Name, nameInvalid_);
}

void SplashScene::render(sf::RenderTarget &window) const {
    for (const auto &decoration : decorations_) {
        drawPanel(window, decoration.tile);
        window.draw(decoration.label);
    }
    window.draw(title_);
    window.draw(subtitle_);
    panelReveal_.draw(window, [&](sf::RenderTarget &panelTarget) {
        drawPanel(panelTarget, formPanel_);
        panelTarget.draw(nameLabel_);
        drawTextInput(panelTarget, nameBox_, nameText_);
        drawButton(panelTarget, startButton_, startText_, ButtonIcon::Play, startFeedback_);
        drawButton(panelTarget, scoresButton_, scoresText_, ButtonIcon::Trophy, scoresFeedback_);
        panelTarget.draw(validationText_);
    });
}

bool SplashScene::isWhitespace(const sf::Uint32 codePoint) {
    return isNameWhitespace(codePoint);
}

sf::String SplashScene::trimInput(const sf::String &value) {
    std::size_t begin = 0;
    while (begin < value.getSize() && isWhitespace(value[begin])) {
        ++begin;
    }

    std::size_t end = value.getSize();
    while (end > begin && isWhitespace(value[end - 1])) {
        --end;
    }

    return value.substring(begin, end - begin);
}

std::string SplashScene::toUtf8(const sf::String &value) {
    const auto utf8 = value.toUtf8();
    return std::string(utf8.begin(), utf8.end());
}

void SplashScene::refreshNameText() {
    const sf::String trimmedName = trimInput(nameInput_);
    playerName_ = toUtf8(trimmedName);
    if (playerName_.empty()) {
        nameText_.setString(localizedText(*nameText_.getFont(), "Adınızı yazın", "Adinizi yazin"));
        styleText(nameText_, theme::colors::kTextSecondary);
        setNameInvalid(nameInvalid_);
    } else {
        const auto presentation = presentPlayerName(*nameText_.getFont(), trimmedName);
        nameText_.setString(presentation.text);
        styleText(nameText_, theme::colors::kTextPrimary);
        auto measuredText = nameText_;
        const float availableWidth = nameBox_.getSize().x - theme::spacing::kXl;
        const auto fit =
            fitTextWidth(measuredText, availableWidth, theme::typography::minimum::kInput);
        if (presentation.hasReplacements) {
            setNameInvalid(true, NameIssue::Unsupported);
        } else if (!fit.meetsMinimum) {
            setNameInvalid(true, NameIssue::TooWide);
        } else {
            setNameInvalid(false);
        }
        if (!fit.meetsMinimum) {
            // Keep an invalid wide edit readable without changing its original buffer.
            fitSingleLineText(nameText_, availableWidth);
        }
    }
    if (nameLimitReached_ && !nameInvalid_) {
        validationText_.setString(localizedText(*validationText_.getFont(),
                                                "En fazla 18 karakter girebilirsiniz.",
                                                "En fazla 18 karakter girebilirsiniz."));
        styleText(validationText_, theme::colors::kTextSecondary);
        centerTextOrigin(validationText_);
    }
    centerTextOrigin(nameText_);
    nameText_.setPosition(nameBox_.getPosition());
}

void SplashScene::setNameInvalid(const bool invalid, const NameIssue issue) {
    nameInvalid_ = invalid;
    if (invalid) {
        const auto &font = *validationText_.getFont();
        switch (issue) {
        case NameIssue::Empty:
            validationText_.setString(
                localizedText(font, "Lütfen adınızı girin.", "Lutfen adinizi girin."));
            break;
        case NameIssue::Unsupported:
            validationText_.setString(localizedText(font, "Adınızda gösterilemeyen karakter var.",
                                                    "Adinizda gosterilemeyen karakter var."));
            break;
        case NameIssue::TooWide:
            validationText_.setString(localizedText(font, "Adınız ekrana sığmıyor. Kısaltın.",
                                                    "Adiniz ekrana sigmiyor. Kisaltin."));
            break;
        }
        styleText(validationText_, theme::colors::kError);
        centerTextOrigin(validationText_);
    } else {
        validationText_.setString("");
    }
    updateTextInputStyle(nameBox_, focus_ == FocusTarget::Name, nameInvalid_);
}

void SplashScene::setFocus(const FocusTarget focus) {
    focus_ = focus;
    pendingActivation_.reset();
    updateTextInputStyle(nameBox_, focus_ == FocusTarget::Name, nameInvalid_);
    updateButtonFocus(startButton_, focus_ == FocusTarget::Start, startFeedback_);
    updateButtonFocus(scoresButton_, focus_ == FocusTarget::Scores, scoresFeedback_);
    updateButtonHover(startButton_, focus_ == FocusTarget::Start, ButtonVariant::Primary,
                      startFeedback_);
    updateButtonHover(scoresButton_, focus_ == FocusTarget::Scores, ButtonVariant::Secondary,
                      scoresFeedback_);
}

void SplashScene::moveFocus(const bool backwards) {
    switch (focus_) {
    case FocusTarget::None:
        setFocus(backwards ? FocusTarget::Scores : FocusTarget::Name);
        break;
    case FocusTarget::Name:
        setFocus(backwards ? FocusTarget::Scores : FocusTarget::Start);
        break;
    case FocusTarget::Start:
        setFocus(backwards ? FocusTarget::Name : FocusTarget::Scores);
        break;
    case FocusTarget::Scores:
        setFocus(backwards ? FocusTarget::Start : FocusTarget::Name);
        break;
    }
}

SceneCommand SplashScene::validateAndStart() {
    refreshNameText();
    if (playerName_.empty()) {
        setFocus(FocusTarget::Name);
        setNameInvalid(true);
        return SceneCommand::None;
    }

    if (nameInvalid_) {
        setFocus(FocusTarget::Name);
        return SceneCommand::None;
    }

    setNameInvalid(false);
    setFocus(FocusTarget::None);
    return SceneCommand::StartGame;
}

std::vector<AccessibilityNode> SplashScene::accessibilityNodes() const {
    auto field = accessibleNode("name", AccessibilityRole::TextField, "Oyuncu Adı",
                                nameBox_.getGlobalBounds());
    field.value = toUtf8(nameInput_);
    field.help = "Zorunlu; en fazla 18 karakter.";
    field.error =
        nameInvalid_ || nameLimitReached_ ? accessibleUtf8(validationText_.getString()) : "";
    field.required = field.editable = field.focusable = true;
    field.focused = focus_ == FocusTarget::Name;
    field.actions = {AccessibilityActionKind::Focus, AccessibilityActionKind::SetValue};
    return {
        accessibleNode("title", AccessibilityRole::StaticText, accessibleUtf8(title_.getString()),
                       title_.getGlobalBounds()),
        accessibleNode("description", AccessibilityRole::StaticText,
                       accessibleUtf8(subtitle_.getString()), subtitle_.getGlobalBounds()),
        field,
        accessibleButton("start", "Oyuna Başla",
                         controlBounds(startButton_, theme::sizes::kButtonBorder),
                         focus_ == FocusTarget::Start),
        accessibleButton("scores", "Skorlar",
                         controlBounds(scoresButton_, theme::sizes::kButtonBorder),
                         focus_ == FocusTarget::Scores)};
}

bool SplashScene::focusAccessible(const std::string_view id) {
    if (id == "name")
        setFocus(FocusTarget::Name);
    else if (id == "start")
        setFocus(FocusTarget::Start);
    else if (id == "scores")
        setFocus(FocusTarget::Scores);
    else
        return false;
    return true;
}

bool SplashScene::setAccessiblePlayerName(const std::string_view value) {
    const auto text = toUnicode(value);
    if (toUtf8(text) != value)
        return false;
    for (const auto code : text) {
        if (code < 32U || code == 127U || code > 0x10FFFFU || (code >= 0xD800U && code <= 0xDFFFU))
            return false;
    }
    setFocus(FocusTarget::Name);
    if (text.getSize() > kMaxNameLength) {
        nameLimitReached_ = true;
        refreshNameText();
        return false;
    }
    nameInput_ = text;
    nameLimitReached_ = false;
    refreshNameText();
    return true;
}

SceneCommand SplashScene::activateAccessible(const std::string_view id) {
    if (id != "start" && id != "scores")
        return SceneCommand::None;
    resetInputState();
    if (id == "start")
        return validateAndStart();
    setNameInvalid(false);
    return SceneCommand::ShowHighScores;
}

} // namespace app::ui
