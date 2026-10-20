#include "app/ui/PlayingScene.hpp"

#include <algorithm>
#include <cmath>

#include <string>

namespace app::ui {

namespace {

float easeTileProgress(const float progress) {
    const float t = clamp01(progress);
    // Smoothstep keeps both endpoints exact, with no sudden start or stop.
    return t * t * (3.f - 2.f * t);
}

float mergePopAmplitude(const numtilo_core::Cell &cell,
                        const std::vector<numtilo_core::TileMerge> &merges) {
    const bool hasAdjacentMerge = std::any_of(merges.begin(), merges.end(), [&](const auto &merge) {
        return std::abs(merge.cell.row - cell.row) + std::abs(merge.cell.col - cell.col) == 1;
    });
    if (!hasAdjacentMerge) {
        return theme::motion::kMergePopAmplitude;
    }
    // Preserve a visible gap between simultaneous neighbors throughout the pulse.
    const float separatedPeak =
        (static_cast<float>(kCellSize + kPadding) - theme::spacing::kXs) / kCellSize;
    return std::min(theme::motion::kMergePopAmplitude, separatedPeak - 1.f);
}

struct PlayingLayout {
    sf::FloatRect sidePanel;
    sf::FloatRect boardBounds;
    sf::Vector2f boardOffset;
    sf::FloatRect scoreCard;
    sf::FloatRect bestCard;
};

PlayingLayout playingLayout(const float width) {
    const float height =
        static_cast<float>(theme::sizes::kWindowHeight) - 2.f * theme::sizes::kWindowMargin;
    const float extent = theme::sizes::kPlayingBoardAreaExtent;
    const float sideWidth = theme::sizes::kPlayingSidePanelWidth;
    const float gap = theme::sizes::kPlayingColumnGap;
    const float boardWidth = width - sideWidth - gap;
    const float top = (height - extent) / 2.f;
    const float tileExtent = static_cast<float>(kGridSize * kCellSize + (kGridSize + 1) * kPadding);
    const sf::FloatRect boardBounds((boardWidth - tileExtent) / 2.f,
                                    top + (extent - tileExtent) / 2.f, tileExtent, tileExtent);
    const float padding = theme::spacing::kXl;
    const float cardWidth = (sideWidth - 2.f * padding - theme::spacing::kLg) / 2.f;
    const float cardTop = top + padding + theme::typography::kPlayingTitleSize +
                          theme::spacing::kSm + theme::sizes::kPlayingDescriptionHeight +
                          theme::spacing::kLg + theme::typography::kCaptionSize +
                          theme::spacing::kSm + theme::spacing::kXl + padding;
    const sf::FloatRect scoreCard(boardWidth + gap + padding, cardTop, cardWidth,
                                  theme::sizes::kPlayingScoreCardHeight);
    return {
        {boardWidth + gap, top, sideWidth, extent},
        boardBounds,
        {boardBounds.left, boardBounds.top - static_cast<float>(kTopPanelHeight)},
        scoreCard,
        {scoreCard.left + cardWidth + theme::spacing::kLg, cardTop, cardWidth, scoreCard.height}};
}

void positionTextTopLeft(sf::Text &text, const float x, const float y) {
    const auto bounds = text.getLocalBounds();
    text.setOrigin(bounds.left, bounds.top);
    text.setPosition(x, y);
}

} // namespace

PlayingScene::PlayingScene(const sf::Font &font)
    : title_("Numtilo", font, theme::typography::kPlayingTitleSize),
      description_(localizedText(font, "Sayıları birleştir,\n2048'e ulaş!",
                                 "Sayilari birlestir,\n2048'e ulas!"),
                   font, theme::typography::kPlayingDescriptionSize),
      playerLabel_("Oyuncu", font, theme::typography::kCaptionSize),
      playerName_("", font, theme::typography::kPlayingPlayerNameSize),
      newGameButton_({0.f, theme::sizes::kPlayingActionHeight}),
      soundRow_(createPanel({0.f, theme::sizes::kPlayingSoundRowHeight})),
      mainMenuButton_({0.f, theme::sizes::kPlayingActionHeight}),
      newGameText_("Yeni Oyun", font, theme::typography::kButtonSize),
      soundText_("", font, theme::typography::kLabelSize),
      mainMenuText_(localizedText(font, "Ana Menü", "Ana Menu"), font,
                    theme::typography::kButtonSize) {
    styleText(title_, theme::colors::kTextPrimary, theme::typography::kDisplayStyle);
    styleText(description_, theme::colors::kTextSecondary);
    styleText(playerLabel_, theme::colors::kTextSecondary);
    styleText(playerName_, theme::colors::kTextPrimary, theme::typography::kEmphasisStyle);
    styleText(soundText_, theme::colors::kTextPrimary);
    styleButton(newGameButton_, newGameText_, ButtonVariant::Primary);
    styleButton(mainMenuButton_, mainMenuText_, ButtonVariant::Secondary);
    centerTextOrigin(newGameText_);
    centerTextOrigin(mainMenuText_);

    setSoundEnabled(true);
}

SceneAction PlayingScene::handleEvent(const sf::Event &event, const sf::RenderWindow &window,
                                      const float width) {
    tickVisuals();
    layoutControls(width);

    const bool newGameActivated = newGameFeedback_.handleEvent(event, window, newGameButton_,
                                                               focus_ == FocusTarget::NewGame, 0.f);
    const bool mainMenuActivated = mainMenuFeedback_.handleEvent(
        event, window, mainMenuButton_, focus_ == FocusTarget::MainMenu, 0.f);
    const bool soundActivated = soundPointer_.handleEvent(event, [&](const sf::Vector2i &pixel) {
        return controlBounds(soundRow_, theme::sizes::kPanelBorder)
            .contains(window.mapPixelToCoords(pixel));
    });

    if (isPrimaryMousePress(event)) {
        pendingActivation_.reset();
    }

    if (event.type == sf::Event::LostFocus) {
        setFocus(FocusTarget::None);
        return {SceneCommand::None, std::nullopt};
    }

    if (event.type == sf::Event::KeyReleased) {
        if (pendingActivation_ == event.key.code) {
            pendingActivation_.reset();
            return {activateControl(focus_), std::nullopt};
        }
        return {SceneCommand::None, std::nullopt};
    }

    if (event.type == sf::Event::KeyPressed &&
        (event.key.alt || event.key.control || event.key.system)) {
        return {SceneCommand::None, std::nullopt};
    }

    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Tab) {
        moveFocus(event.key.shift);
        return {SceneCommand::None, std::nullopt};
    }

    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
        return {activateControl(FocusTarget::MainMenu), std::nullopt};
    }

    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::N) {
        return {activateControl(FocusTarget::NewGame), std::nullopt};
    }

    if (event.type == sf::Event::KeyPressed &&
        (event.key.code == sf::Keyboard::Enter || event.key.code == sf::Keyboard::Space)) {
        if (boardAccessibleFocused_)
            return {SceneCommand::None, std::nullopt};
        if (focus_ != FocusTarget::None) {
            pendingActivation_ = event.key.code;
            return {SceneCommand::None, std::nullopt};
        }
        if (event.key.code == sf::Keyboard::Enter) {
            return {activateControl(FocusTarget::NewGame), std::nullopt};
        }
        return {SceneCommand::None, std::nullopt};
    }

    if (isPrimaryMouseRelease(event)) {
        setFocus(FocusTarget::None);
        if (newGameActivated) {
            return {activateControl(FocusTarget::NewGame), std::nullopt};
        }
        if (mainMenuActivated) {
            return {activateControl(FocusTarget::MainMenu), std::nullopt};
        }
        if (soundActivated) {
            return {activateControl(FocusTarget::Sound), std::nullopt};
        }
    }

    if (moveAnimationActive_) {
        return {SceneCommand::None, std::nullopt};
    }

    if (event.type != sf::Event::KeyPressed) {
        return {SceneCommand::None, std::nullopt};
    }

    const auto direction = mapDirection(event.key.code);
    if (!direction.has_value()) {
        return {SceneCommand::None, std::nullopt};
    }

    setFocus(FocusTarget::None);
    return {SceneCommand::None, direction};
}

void PlayingScene::setFocus(const FocusTarget focus) {
    boardAccessibleFocused_ = false;
    focus_ = focus;
    pendingActivation_.reset();
    updateButtonFocus(newGameButton_, focus_ == FocusTarget::NewGame, newGameFeedback_);
    updateButtonFocus(mainMenuButton_, focus_ == FocusTarget::MainMenu, mainMenuFeedback_);
    if (focus_ == FocusTarget::Sound) {
        updateButtonFocus(soundRow_, true);
    } else {
        stylePanel(soundRow_);
    }
    updateButtonHover(newGameButton_, focus_ == FocusTarget::NewGame, ButtonVariant::Primary,
                      newGameFeedback_);
    updateButtonHover(mainMenuButton_, focus_ == FocusTarget::MainMenu, ButtonVariant::Secondary,
                      mainMenuFeedback_);
    soundHovered_ = focus_ == FocusTarget::Sound;
}

void PlayingScene::moveFocus(const bool backwards) {
    switch (focus_) {
    case FocusTarget::None:
        setFocus(backwards ? FocusTarget::MainMenu : FocusTarget::NewGame);
        break;
    case FocusTarget::NewGame:
        setFocus(backwards ? FocusTarget::MainMenu : FocusTarget::Sound);
        break;
    case FocusTarget::Sound:
        setFocus(backwards ? FocusTarget::NewGame : FocusTarget::MainMenu);
        break;
    case FocusTarget::MainMenu:
        setFocus(backwards ? FocusTarget::Sound : FocusTarget::NewGame);
        break;
    }
}

SceneCommand PlayingScene::activateControl(const FocusTarget target) {
    switch (target) {
    case FocusTarget::NewGame:
        return SceneCommand::RestartGame;
    case FocusTarget::Sound:
        return SceneCommand::ToggleSound;
    case FocusTarget::MainMenu:
        return SceneCommand::ShowSplash;
    case FocusTarget::None:
        return SceneCommand::None;
    }
    return SceneCommand::None;
}

void PlayingScene::setSoundEnabled(const bool enabled) {
    soundEnabled_ = enabled;
    soundText_.setString(localizedText(*soundText_.getFont(),
                                       soundEnabled_ ? "Ses Açık" : "Ses Kapalı",
                                       soundEnabled_ ? "Ses Acik" : "Ses Kapali"));
    centerTextOrigin(soundText_);
}

void PlayingScene::updateHover(const sf::Vector2f &mousePos, const float width) {
    layoutControls(width);
    updateButtonHover(
        newGameButton_,
        focus_ == FocusTarget::NewGame ||
            controlBounds(newGameButton_, theme::sizes::kButtonBorder).contains(mousePos),
        ButtonVariant::Primary, newGameFeedback_);
    updateButtonHover(
        mainMenuButton_,
        focus_ == FocusTarget::MainMenu ||
            controlBounds(mainMenuButton_, theme::sizes::kButtonBorder).contains(mousePos),
        ButtonVariant::Secondary, mainMenuFeedback_);
    soundHovered_ = focus_ == FocusTarget::Sound ||
                    controlBounds(soundRow_, theme::sizes::kPanelBorder).contains(mousePos);
}

void PlayingScene::tickVisuals() {
    const auto now = Clock::now();
    updateMoveAnimationState(now);
    updateFloatingScores(now);
}

bool PlayingScene::hasActiveAnimations() const {
    return moveAnimationActive_;
}

void PlayingScene::resetVisualEffects() {
    newGameFeedback_.reset();
    mainMenuFeedback_.reset();
    soundPointer_.reset();

    setFocus(FocusTarget::None);
    moveAnimationActive_ = false;
    movingTiles_.clear();
    mergeCells_.clear();
    hiddenDuringSlide_.clear();
    hiddenDuringSpawn_.clear();
    spawnedTile_.reset();
    floatingScores_.clear();
}

void PlayingScene::onEnter() {
    panelReveal_.restart();
}

void PlayingScene::resetInputState() {
    newGameFeedback_.reset();
    mainMenuFeedback_.reset();
    soundPointer_.reset();

    setFocus(FocusTarget::None);
}

void PlayingScene::render(sf::RenderTarget &window, const sf::Font &font,
                          const numtilo_core::Game &game, const float width, const int bestScore,
                          const std::string_view playerName) {
    tickVisuals();
    layoutControls(width);

    const auto layout = playingLayout(width);
    const float panelInset = theme::sizes::kPanelBorder;
    auto boardBg = createPanel({layout.boardBounds.width - 2.f * panelInset,
                                layout.boardBounds.height - 2.f * panelInset});
    boardBg.setPosition(layout.boardBounds.left + panelInset, layout.boardBounds.top + panelInset);
    boardBg.setFillColor(theme::colors::kBoard);
    boardBg.setCornersRadius(theme::radius::kBoard);
    drawPanel(window, boardBg);
    const auto boardCenter = [&](const numtilo_core::Cell &cell) {
        return cellCenter(cell) + layout.boardOffset;
    };

    const auto now = Clock::now();
    const float elapsed = moveAnimationActive_
                              ? std::chrono::duration<float>(now - moveAnimationStart_).count()
                              : 0.f;
    const bool inSlideStage = moveAnimationActive_ && elapsed < kSlideAnimationDuration;
    const bool inSpawnStage = moveAnimationActive_ && spawnedTile_.has_value() &&
                              elapsed >= kSlideAnimationDuration &&
                              elapsed < (kSlideAnimationDuration + kSpawnFadeDuration);
    const bool hideSpawnTile = moveAnimationActive_ && spawnedTile_.has_value() &&
                               elapsed < (kSlideAnimationDuration + kSpawnFadeDuration);
    const bool inMergeStage = moveAnimationActive_ && !mergeCells_.empty() &&
                              elapsed >= kSlideAnimationDuration &&
                              elapsed < (kSlideAnimationDuration + kMergePopDuration);

    const auto &grid = game.getGrid();
    for (int row = 0; row < kGridSize; ++row) {
        for (int col = 0; col < kGridSize; ++col) {
            const numtilo_core::Cell cell{row, col};
            const sf::Vector2f center = boardCenter(cell);
            drawEmptyCell(window, center);

            const int value = grid[row][col];
            if (value == 0) {
                continue;
            }

            bool hideValue = false;
            if (inSlideStage && hiddenDuringSlide_.contains(cell)) {
                hideValue = true;
            }
            if (!hideValue && hideSpawnTile && hiddenDuringSpawn_.contains(cell)) {
                hideValue = true;
            }
            if (hideValue) {
                continue;
            }

            float scale = 1.f;
            if (inMergeStage) {
                for (const auto &mergeCell : mergeCells_) {
                    if (mergeCell.cell.row == row && mergeCell.cell.col == col) {
                        const float mergeProgress = easeTileProgress(
                            (elapsed - kSlideAnimationDuration) / kMergePopDuration);
                        scale = 1.f + mergePopAmplitude(cell, mergeCells_) *
                                          std::sin(mergeProgress * kPi);
                        break;
                    }
                }
            }

            drawTile(window, font, value, center, scale);
        }
    }

    if (inSlideStage) {
        const float slideProgress = easeTileProgress(elapsed / kSlideAnimationDuration);
        for (const auto &tile : movingTiles_) {
            const auto start = boardCenter(tile.from);
            const auto end = boardCenter(tile.to);
            drawTile(window, font, tile.value, lerp(start, end, slideProgress));
        }
    }

    if (inSpawnStage && spawnedTile_.has_value()) {
        const float spawnProgress =
            easeTileProgress((elapsed - kSlideAnimationDuration) / kSpawnFadeDuration);
        const sf::Vector2f spawnCenter =
            boardCenter(numtilo_core::Cell{spawnedTile_->row, spawnedTile_->col});
        drawTile(window, font, spawnedTile_->value, spawnCenter, 0.82f + (0.18f * spawnProgress),
                 toAlpha(spawnProgress));
    }

    panelReveal_.draw(window, [&](sf::RenderTarget &panelTarget) {
        auto panelBg = createPanel(
            {layout.sidePanel.width - 2.f * panelInset, layout.sidePanel.height - 2.f * panelInset},
            PanelVariant::Side);
        panelBg.setPosition(layout.sidePanel.left + panelInset, layout.sidePanel.top + panelInset);
        drawPanel(panelTarget, panelBg);
        const float textLeft = layout.sidePanel.left + theme::spacing::kXl;
        const float textWidth = layout.sidePanel.width - 2.f * theme::spacing::kXl;
        float textTop = layout.sidePanel.top + theme::spacing::kXl;
        positionTextTopLeft(title_, textLeft, textTop);
        panelTarget.draw(title_);
        textTop += theme::typography::kPlayingTitleSize + theme::spacing::kSm;
        positionTextTopLeft(description_, textLeft, textTop);
        panelTarget.draw(description_);
        textTop += theme::sizes::kPlayingDescriptionHeight + theme::spacing::kLg;
        const auto presentation =
            presentPlayerName(font, toUnicode(playerName.empty() ? "Oyuncu" : playerName));
        playerLabel_.setString(
            presentation.hasReplacements
                ? localizedText(font, "Oyuncu (gösterilemeyen: ?)", "Oyuncu (gosterilemeyen: ?)")
                : sf::String("Oyuncu"));
        fitTextWidth(playerLabel_, textWidth, theme::typography::minimum::kCaption);
        positionTextTopLeft(playerLabel_, textLeft, textTop);
        panelTarget.draw(playerLabel_);
        textTop += theme::typography::kCaptionSize + theme::spacing::kSm;
        playerName_.setString(presentation.text);
        fitSingleLineText(playerName_, textWidth);
        positionTextTopLeft(playerName_, textLeft, textTop);
        panelTarget.draw(playerName_);

        drawScoreCard(panelTarget, font, layout.scoreCard, "Skor",
                      sf::String(std::to_string(game.getScore())));
        drawScoreCard(panelTarget, font, layout.bestCard, localizedText(font, "En İyi", "En Iyi"),
                      sf::String(std::to_string(bestScore)));

        renderFloatingScores(panelTarget, font, layout.scoreCard);

        drawButton(panelTarget, newGameButton_, newGameText_, ButtonIcon::Restart,
                   newGameFeedback_);
        drawPanel(panelTarget, soundRow_);
        panelTarget.draw(soundText_);
        drawToggle(
            panelTarget,
            {soundRow_.getPosition().x + soundRow_.getSize().x - theme::spacing::kMd -
                 theme::sizes::kToggle.x,
             soundRow_.getPosition().y + (soundRow_.getSize().y - theme::sizes::kToggle.y) / 2.f},
            soundEnabled_, soundHovered_);
        drawButton(panelTarget, mainMenuButton_, mainMenuText_, ButtonIcon::Home,
                   mainMenuFeedback_);
    });
}

void PlayingScene::onMove(const numtilo_core::MoveResult &result) {
    movingTiles_ = result.movements;
    mergeCells_ = result.merges;
    spawnedTile_ = result.spawnedTile;

    hiddenDuringSlide_.clear();
    for (const auto &tile : movingTiles_) {
        hiddenDuringSlide_.insert(tile.to);
    }
    if (spawnedTile_.has_value()) {
        hiddenDuringSlide_.insert(numtilo_core::Cell{spawnedTile_->row, spawnedTile_->col});
        hiddenDuringSpawn_.clear();
        hiddenDuringSpawn_.insert(numtilo_core::Cell{spawnedTile_->row, spawnedTile_->col});
    } else {
        hiddenDuringSpawn_.clear();
    }

    if (result.scoreDelta > 0) {
        floatingScores_.push_back(FloatingScoreEffect{result.scoreDelta, Clock::now()});
    }

    moveAnimationStart_ = Clock::now();
    moveAnimationActive_ = true;
}

void PlayingScene::layoutControls(const float width) {
    const auto layout = playingLayout(width);
    constexpr float panelPadding = theme::spacing::kXl;

    const float controlWidth = layout.sidePanel.width - 2.f * panelPadding;
    const float left = layout.sidePanel.left + panelPadding;
    float top = layout.sidePanel.top + layout.sidePanel.height - panelPadding -
                2.f * theme::sizes::kPlayingActionHeight - theme::sizes::kPlayingSoundRowHeight -
                2.f * theme::spacing::kSm;
    newGameButton_.setSize({controlWidth, theme::sizes::kPlayingActionHeight});
    newGameButton_.setPosition(left, top);
    newGameText_.setPosition(left + controlWidth / 2.f,
                             top + theme::sizes::kPlayingActionHeight / 2.f);
    top += theme::sizes::kPlayingActionHeight + theme::spacing::kSm;
    soundRow_.setSize({controlWidth, theme::sizes::kPlayingSoundRowHeight});
    soundRow_.setPosition(left, top);
    fitTextWidth(soundText_, controlWidth - 3.f * theme::spacing::kMd - theme::sizes::kToggle.x);
    soundText_.setOrigin(soundText_.getLocalBounds().left, soundText_.getOrigin().y);
    soundText_.setPosition(left + theme::spacing::kMd,
                           top + theme::sizes::kPlayingSoundRowHeight / 2.f);
    top += theme::sizes::kPlayingSoundRowHeight + theme::spacing::kSm;
    mainMenuButton_.setSize({controlWidth, theme::sizes::kPlayingActionHeight});
    mainMenuButton_.setPosition(left, top);
    mainMenuText_.setPosition(left + controlWidth / 2.f,
                              top + theme::sizes::kPlayingActionHeight / 2.f);
}

void PlayingScene::updateMoveAnimationState(const Clock::time_point now) {
    if (!moveAnimationActive_) {
        return;
    }

    const float elapsed = std::chrono::duration<float>(now - moveAnimationStart_).count();
    const float totalDuration =
        std::max(kSlideAnimationDuration + (mergeCells_.empty() ? 0.f : kMergePopDuration),
                 kSlideAnimationDuration + (spawnedTile_.has_value() ? kSpawnFadeDuration : 0.f));

    if (elapsed < totalDuration) {
        return;
    }

    moveAnimationActive_ = false;
    movingTiles_.clear();
    mergeCells_.clear();
    hiddenDuringSlide_.clear();
    hiddenDuringSpawn_.clear();
    spawnedTile_.reset();
}

void PlayingScene::updateFloatingScores(const Clock::time_point now) {
    floatingScores_.erase(
        std::remove_if(floatingScores_.begin(), floatingScores_.end(),
                       [&](const auto &effect) {
                           const float elapsed =
                               std::chrono::duration<float>(now - effect.startedAt).count();
                           return elapsed >= kFloatingScoreDuration;
                       }),
        floatingScores_.end());
}

TextFitResult PlayingScene::renderFloatingScores(sf::RenderTarget &window, const sf::Font &font,
                                                 const sf::FloatRect scoreCard) {
    if (floatingScores_.empty()) {
        return {0.f, true};
    }
    // The total remains in the card; show the latest move's delta without
    // stacking several labels in the same reserved feedback space.
    const auto &effect = floatingScores_.back();
    const float elapsed = std::chrono::duration<float>(Clock::now() - effect.startedAt).count();
    const float progress = clamp01(elapsed / kFloatingScoreDuration);
    const float remaining = 1.f - progress;
    const float fade = clamp01((progress - theme::motion::kScoreFeedbackHoldFraction) /
                               (1.f - theme::motion::kScoreFeedbackHoldFraction));

    sf::Text scoreDeltaText("+" + std::to_string(effect.scoreDelta), font,
                            theme::typography::kScoreFeedbackSize);
    auto color = theme::colors::kScoreIncrease;
    color.a = toAlpha(1.f - fade * fade * (3.f - 2.f * fade));
    styleText(scoreDeltaText, color, theme::typography::kScoreFeedbackStyle);
    const auto fit =
        fitTextWidth(scoreDeltaText, scoreCard.width - 2.f * theme::sizes::kScoreFeedbackTextInset,
                     theme::typography::minimum::kScoreFeedback);
    centerTextOrigin(scoreDeltaText);
    scoreDeltaText.setPosition(scoreCard.left + scoreCard.width / 2.f,
                               scoreCard.top + scoreCard.height + theme::spacing::kXs +
                                   scoreDeltaText.getGlobalBounds().height / 2.f +
                                   theme::motion::kScoreFeedbackTravel * remaining * remaining *
                                       remaining);
    window.draw(scoreDeltaText);
    return fit;
}

std::vector<AccessibilityNode> PlayingScene::accessibilityNodes(const numtilo_core::Game &game,
                                                                const float width,
                                                                const int bestScore,
                                                                const std::string_view playerName) {
    layoutControls(width);
    const auto layout = playingLayout(width);
    std::vector<AccessibilityNode> nodes;
    auto board =
        accessibleNode("board", AccessibilityRole::Table, "Oyun tahtası", layout.boardBounds);
    board.rowCount = board.columnCount = 4;
    board.focusable = true;
    board.focused = boardAccessibleFocused_;
    board.actions = {AccessibilityActionKind::Focus};
    board.help = "Dört satır, dört sütun. Yön işlemleriyle taşları birleştirin.";
    if (!moveAnimationActive_)
        board.customActions = {{"Sola oyna", AccessibilityActionKind::MoveLeft},
                               {"Sağa oyna", AccessibilityActionKind::MoveRight},
                               {"Yukarı oyna", AccessibilityActionKind::MoveUp},
                               {"Aşağı oyna", AccessibilityActionKind::MoveDown}};
    nodes.push_back(board);
    const auto grid = game.getGrid();
    for (int row = 0; row < 4; ++row) {
        const auto rowId = "board-row-" + std::to_string(row);
        auto rowNode =
            accessibleNode(rowId, AccessibilityRole::Row, "Satır " + std::to_string(row + 1),
                           {layout.boardBounds.left,
                            layout.boardBounds.top + kPadding + row * (kCellSize + kPadding),
                            layout.boardBounds.width, static_cast<float>(kCellSize)},
                           "board");
        rowNode.row = row;
        nodes.push_back(rowNode);
        for (int col = 0; col < 4; ++col) {
            auto cell = accessibleNode(
                "cell-" + std::to_string(row) + "-" + std::to_string(col), AccessibilityRole::Cell,
                "Satır " + std::to_string(row + 1) + ", sütun " + std::to_string(col + 1),
                {layout.boardBounds.left + kPadding + col * (kCellSize + kPadding),
                 layout.boardBounds.top + kPadding + row * (kCellSize + kPadding),
                 static_cast<float>(kCellSize), static_cast<float>(kCellSize)},
                rowId);
            cell.value = grid[row][col] == 0 ? "Boş" : std::to_string(grid[row][col]);
            cell.row = row;
            cell.column = col;
            nodes.push_back(cell);
        }
    }
    nodes.push_back(accessibleNode("playing-panel", AccessibilityRole::Group,
                                   "Oyun bilgileri ve kontroller", layout.sidePanel));
    auto player = accessibleNode(
        "player", AccessibilityRole::StaticText, "Oyuncu",
        {layout.sidePanel.left + theme::spacing::kXl,
         layout.sidePanel.top + theme::spacing::kXl + theme::typography::kPlayingTitleSize +
             theme::spacing::kSm + theme::sizes::kPlayingDescriptionHeight + theme::spacing::kLg +
             theme::typography::kCaptionSize + theme::spacing::kSm,
         layout.sidePanel.width - 2.f * theme::spacing::kXl,
         static_cast<float>(theme::typography::kPlayingPlayerNameSize)},
        "playing-panel");
    player.value = std::string(playerName);
    nodes.push_back(player);
    auto score = accessibleNode("score", AccessibilityRole::StaticText, "Skor", layout.scoreCard,
                                "playing-panel");
    score.value = std::to_string(game.getScore());
    nodes.push_back(score);
    auto best = accessibleNode("best", AccessibilityRole::StaticText, "En İyi", layout.bestCard,
                               "playing-panel");
    best.value = std::to_string(bestScore);
    nodes.push_back(best);
    auto newGame = accessibleButton("new", "Yeni Oyun",
                                    controlBounds(newGameButton_, theme::sizes::kButtonBorder),
                                    focus_ == FocusTarget::NewGame);
    newGame.parentId = "playing-panel";
    nodes.push_back(newGame);
    auto sound =
        accessibleButton("sound", "Ses", controlBounds(soundRow_, theme::sizes::kPanelBorder),
                         focus_ == FocusTarget::Sound);
    sound.parentId = "playing-panel";
    sound.role = AccessibilityRole::Toggle;
    sound.actions.push_back(AccessibilityActionKind::SetValue);
    sound.checked = soundEnabled_;
    sound.value = soundEnabled_ ? "Açık" : "Kapalı";
    nodes.push_back(sound);
    auto menu = accessibleButton("menu", "Ana Menü",
                                 controlBounds(mainMenuButton_, theme::sizes::kButtonBorder),
                                 focus_ == FocusTarget::MainMenu);
    menu.parentId = "playing-panel";
    nodes.push_back(menu);
    return nodes;
}

bool PlayingScene::focusAccessible(const std::string_view id) {
    if (id == "board") {
        setFocus(FocusTarget::None);
        boardAccessibleFocused_ = true;
    } else if (id == "new")
        setFocus(FocusTarget::NewGame);
    else if (id == "sound")
        setFocus(FocusTarget::Sound);
    else if (id == "menu")
        setFocus(FocusTarget::MainMenu);
    else
        return false;
    return true;
}

SceneAction PlayingScene::activateAccessible(const std::string_view id) {
    tickVisuals();
    if (id == "new" || id == "sound" || id == "menu") {
        resetInputState();
        const auto target = id == "new"     ? FocusTarget::NewGame
                            : id == "sound" ? FocusTarget::Sound
                                            : FocusTarget::MainMenu;
        setFocus(target);
        return {activateControl(target), std::nullopt};
    }
    if (moveAnimationActive_)
        return {};
    std::optional<numtilo_core::Direction> direction;
    if (id == "left")
        direction = numtilo_core::Direction::Left;
    else if (id == "right")
        direction = numtilo_core::Direction::Right;
    else if (id == "up")
        direction = numtilo_core::Direction::Up;
    else if (id == "down")
        direction = numtilo_core::Direction::Down;
    if (!direction)
        return {};
    const bool boardFocused = boardAccessibleFocused_;
    resetInputState();
    boardAccessibleFocused_ = boardFocused;
    return {SceneCommand::None, direction};
}

} // namespace app::ui
