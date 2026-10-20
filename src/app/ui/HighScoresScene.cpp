#include "app/ui/HighScoresScene.hpp"

#include <algorithm>
#include <string>

namespace app::ui {

HighScoresScene::HighScoresScene(const sf::Font &font, const float width, const float height)
    : font_(font), title_(localizedText(font, "En İyi 5 Skor", "En Iyi 5 Skor"), font,
                          theme::typography::kHeadingSize),
      description_(localizedText(font, "En yüksek skorlara ulaşan oyuncular",
                                 "En yuksek skorlara ulasan oyuncular"),
                   font, theme::typography::kSubtitleSize),
      nameHeader_(localizedText(font, "Oyuncu Adı", "Oyuncu Adi")),
      scoreHeader_(localizedText(font, "Skor", "Skor")),
      emptyText_(localizedText(font, "Henüz skor yok", "Henuz skor yok"), font,
                 theme::typography::kSubtitleSize),
      backText_("Geri", font, theme::typography::kButtonSize),
      backButton_(createButtonForText(backText_, ButtonVariant::Secondary)),
      tableBox_(
          createPanel({std::min(width - 2.f * theme::spacing::kXl, theme::sizes::kLeaderboardWidth),
                       2.f * theme::spacing::kXl + theme::sizes::kLeaderboardHeaderHeight +
                           static_cast<float>(numtilo_core::ScoreManager::kTopScoreCount) *
                               theme::sizes::kLeaderboardRowHeight +
                           static_cast<float>(numtilo_core::ScoreManager::kTopScoreCount - 1U) *
                               theme::spacing::kSm})),
      width_(width), height_(height) {
    styleText(title_, theme::colors::kTextPrimary, theme::typography::kHeadingStyle);
    centerTextOrigin(title_);
    styleText(description_, theme::colors::kTextSecondary);
    centerTextOrigin(description_);

    styleText(emptyText_, theme::colors::kTextSecondary);
    centerTextOrigin(emptyText_);

    styleButton(backButton_, backText_, ButtonVariant::Secondary);
    centerTextOrigin(backText_);
    backButton_.setSize(theme::sizes::kLeaderboardBackButton);
    backButton_.setOrigin(backButton_.getSize() / 2.f);
    const float titleHeight = title_.getLocalBounds().height;
    const float descriptionLineHeight = static_cast<float>(theme::typography::kSubtitleSize);
    const float headerHeight = titleHeight + theme::spacing::kSm + descriptionLineHeight;
    const float groupHeight = headerHeight + theme::spacing::kXl + tableBox_.getSize().y +
                              theme::spacing::kXl + backButton_.getSize().y;
    const float top = (height_ - groupHeight) / 2.f;
    const float iconSize = std::min(theme::sizes::kHeadingIconSize, titleHeight);
    const float iconGap = theme::spacing::kLg;
    const float titleGroupWidth = iconSize + iconGap + title_.getLocalBounds().width;
    trophyBounds_ = {(width_ - titleGroupWidth) / 2.f, top + (titleHeight - iconSize) / 2.f,
                     iconSize, iconSize};
    title_.setPosition(width_ / 2.f + (iconSize + iconGap) / 2.f, top + titleHeight / 2.f);
    description_.setPosition(width_ / 2.f,
                             top + titleHeight + theme::spacing::kSm + descriptionLineHeight / 2.f);
    tableBox_.setPosition((width_ - tableBox_.getSize().x) / 2.f,
                          top + headerHeight + theme::spacing::kXl);
    emptyText_.setPosition(width_ / 2.f, tableBox_.getPosition().y + tableBox_.getSize().y / 2.f);
    backButton_.setPosition(width_ / 2.f, tableBox_.getPosition().y + tableBox_.getSize().y +
                                              theme::spacing::kXl + backButton_.getSize().y / 2.f);
    backText_.setPosition(backButton_.getPosition());
}

SceneCommand HighScoresScene::handleEvent(const sf::Event &event, const sf::RenderWindow &window) {
    const bool backActivated = backFeedback_.handleEvent(event, window, backButton_, backFocused_);

    if (isPrimaryMousePress(event)) {
        pendingActivation_.reset();
    }

    if (event.type == sf::Event::LostFocus) {
        setBackFocused(false);
        return SceneCommand::None;
    }

    if (event.type == sf::Event::KeyReleased) {
        if (pendingActivation_ == event.key.code && backFocused_) {
            setBackFocused(false);
            return SceneCommand::ShowSplash;
        }
        return SceneCommand::None;
    }

    if (event.type == sf::Event::KeyPressed &&
        (event.key.alt || event.key.control || event.key.system)) {
        return SceneCommand::None;
    }

    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Tab) {
        setBackFocused(true);
        return SceneCommand::None;
    }

    if (event.type == sf::Event::KeyPressed && backFocused_ &&
        (event.key.code == sf::Keyboard::Enter || event.key.code == sf::Keyboard::Space)) {
        pendingActivation_ = event.key.code;
        return SceneCommand::None;
    }

    if (event.type == sf::Event::KeyPressed &&
        (event.key.code == sf::Keyboard::Escape || event.key.code == sf::Keyboard::Q ||
         event.key.code == sf::Keyboard::BackSpace)) {
        setBackFocused(false);
        return SceneCommand::ShowSplash;
    }

    if (isPrimaryMouseRelease(event)) {
        setBackFocused(false);
        if (backActivated) {
            return SceneCommand::ShowSplash;
        }
    }
    return SceneCommand::None;
}

void HighScoresScene::onEnter() {
    panelReveal_.restart();
}

void HighScoresScene::resetInputState() {
    backFeedback_.reset();

    setBackFocused(false);
}

void HighScoresScene::updateHover(const sf::Vector2f &mousePos) {
    updateButtonHover(
        backButton_,
        backFocused_ || controlBounds(backButton_, theme::sizes::kButtonBorder).contains(mousePos),
        ButtonVariant::Secondary, backFeedback_);
}

void HighScoresScene::setBackFocused(const bool focused) {
    backFocused_ = focused;
    pendingActivation_.reset();
    updateButtonFocus(backButton_, focused, backFeedback_);
    updateButtonHover(backButton_, focused, ButtonVariant::Secondary, backFeedback_);
}

void HighScoresScene::render(sf::RenderTarget &window,
                             const std::vector<numtilo_core::ScoreEntry> &entries) {
    const std::size_t rowCount =
        std::min(entries.size(), numtilo_core::ScoreManager::kTopScoreCount);
    bool hasReplacements = false;
    for (std::size_t i = 0; i < rowCount; ++i) {
        hasReplacements =
            hasReplacements ||
            presentPlayerName(font_, toUnicode(entries[i].playerName)).hasReplacements;
    }
    description_.setString(
        hasReplacements ? localizedText(font_, "Bazı adlarda karakterler ? ile gösteriliyor.",
                                        "Bazi adlarda karakterler ? ile gosteriliyor.")
                        : localizedText(font_, "En yüksek skorlara ulaşan oyuncular",
                                        "En yuksek skorlara ulasan oyuncular"));
    centerTextOrigin(description_);
    window.draw(title_);
    drawTrophyIcon(window, {trophyBounds_.left, trophyBounds_.top}, trophyBounds_.width,
                   theme::colors::kTrophy);
    window.draw(description_);
    panelReveal_.draw(window, [&](sf::RenderTarget &panelTarget) {
        drawPanel(panelTarget, tableBox_);

        const float left = tableBox_.getPosition().x;
        const float top = tableBox_.getPosition().y;
        const float width = tableBox_.getSize().x;
        const float panelPadding = theme::spacing::kXl;
        const float rowStartY = top + panelPadding + theme::sizes::kLeaderboardHeaderHeight;
        const float rowHeight = theme::sizes::kLeaderboardRowHeight + theme::spacing::kSm;

        if (!entries.empty()) {
            drawLeaderboardHeader(panelTarget, font_,
                                  {left + panelPadding, top + panelPadding,
                                   width - 2.f * panelPadding,
                                   theme::sizes::kLeaderboardHeaderHeight},
                                  nameHeader_, scoreHeader_);
        }

        for (std::size_t i = 0; i < rowCount; ++i) {
            drawLeaderboardRow(panelTarget, font_,
                               {left + panelPadding, rowStartY + static_cast<float>(i) * rowHeight,
                                width - 2.f * panelPadding, theme::sizes::kLeaderboardRowHeight},
                               i, toUnicode(entries[i].playerName),
                               toUnicode(std::to_string(entries[i].score)));
        }

        if (entries.empty()) {
            panelTarget.draw(emptyText_);
        }

        drawButton(panelTarget, backButton_, backText_, ButtonIcon::Back, backFeedback_);
    });
}

std::vector<AccessibilityNode>
HighScoresScene::accessibilityNodes(const std::vector<numtilo_core::ScoreEntry> &entries) const {
    std::vector<AccessibilityNode> nodes;
    nodes.push_back(accessibleNode("title", AccessibilityRole::StaticText, "En İyi 5 Skor",
                                   title_.getGlobalBounds()));
    auto table = accessibleNode("scores-table", AccessibilityRole::Table, "En İyi 5 Skor",
                                tableBox_.getGlobalBounds());
    table.rowCount =
        static_cast<int>(std::min(entries.size(), numtilo_core::ScoreManager::kTopScoreCount));
    table.columnCount = 3;
    nodes.push_back(table);
    if (table.rowCount == 0)
        nodes.push_back(accessibleNode("empty", AccessibilityRole::StaticText, "Henüz skor yok",
                                       tableBox_.getGlobalBounds(), "scores-table"));
    const float left = tableBox_.getPosition().x + theme::spacing::kXl;
    const float top =
        tableBox_.getPosition().y + theme::spacing::kXl + theme::sizes::kLeaderboardHeaderHeight;
    const float width = tableBox_.getSize().x - 2.f * theme::spacing::kXl;
    for (int i = 0; i < table.rowCount; ++i) {
        const auto &entry = entries[static_cast<std::size_t>(i)];
        const auto id = "row-" + std::to_string(i);
        const sf::FloatRect bounds(
            left, top + i * (theme::sizes::kLeaderboardRowHeight + theme::spacing::kSm), width,
            theme::sizes::kLeaderboardRowHeight);
        auto row = accessibleNode(id, AccessibilityRole::Row,
                                  std::to_string(i + 1) + ". " + entry.playerName + ", skor " +
                                      std::to_string(entry.score),
                                  bounds, "scores-table");
        row.row = i;
        nodes.push_back(row);
        auto rank = accessibleNode(id + "-rank", AccessibilityRole::Cell, "Sıra",
                                   {left + theme::spacing::kMd, bounds.top,
                                    theme::sizes::kLeaderboardRankWidth, bounds.height},
                                   id);
        rank.value = std::to_string(i + 1);
        rank.row = i;
        rank.column = 0;
        nodes.push_back(rank);
        auto name = accessibleNode(
            id + "-name", AccessibilityRole::Cell, "Oyuncu Adı",
            {left + theme::spacing::kMd + theme::sizes::kLeaderboardRankWidth, bounds.top,
             width - 3.f * theme::spacing::kMd - theme::sizes::kLeaderboardRankWidth -
                 theme::sizes::kLeaderboardScoreWidth,
             bounds.height},
            id);
        name.value = entry.playerName;
        name.row = i;
        name.column = 1;
        nodes.push_back(name);
        auto score = accessibleNode(
            id + "-score", AccessibilityRole::Cell, "Skor",
            {left + width - theme::spacing::kMd - theme::sizes::kLeaderboardScoreWidth, bounds.top,
             theme::sizes::kLeaderboardScoreWidth, bounds.height},
            id);
        score.value = std::to_string(entry.score);
        score.row = i;
        score.column = 2;
        nodes.push_back(score);
    }
    nodes.push_back(accessibleButton(
        "back", "Geri", controlBounds(backButton_, theme::sizes::kButtonBorder), backFocused_));
    return nodes;
}

bool HighScoresScene::focusAccessible(const std::string_view id) {
    if (id != "back")
        return false;
    setBackFocused(true);
    return true;
}

SceneCommand HighScoresScene::activateAccessible(const std::string_view id) {
    if (id != "back")
        return SceneCommand::None;
    resetInputState();
    return SceneCommand::ShowSplash;
}

} // namespace app::ui
