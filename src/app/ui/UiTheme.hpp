#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Text.hpp>

#include <array>
#include <cstddef>

namespace app::ui::theme::colors {

// Semantic colors for the cream/caramel theme. SFML 2.6 Color constructors
// are not constexpr; inline const keeps these shared values immutable.
inline const sf::Color kBackground{250, 246, 238};
inline const sf::Color kTextPrimary{61, 44, 35};
inline const sf::Color kTextSecondary{113, 95, 81};
inline const sf::Color kTrophy{222, 153, 47};

inline const sf::Color kPrimaryButton{165, 82, 39};
inline const sf::Color kPrimaryButtonHover{179, 96, 49};
inline const sf::Color kOnPrimaryButton{255, 255, 255};

inline const sf::Color kSecondaryButton{235, 218, 196};
inline const sf::Color kSecondaryButtonHover{243, 229, 209};
inline const sf::Color kOnSecondaryButton{61, 44, 35};

inline const sf::Color kInputBackground{255, 251, 245};
inline const sf::Color kInputBorder{153, 132, 108};
inline const sf::Color kInputFocusBorder{165, 82, 39};

inline const sf::Color kError{180, 48, 45};

inline const sf::Color kPanel{247, 241, 229};
inline const sf::Color kPanelBorder{153, 132, 108};
inline const sf::Color kSidePanel{237, 224, 200};
inline const sf::Color kScoreCard{246, 231, 209};
inline const sf::Color kRowEven{249, 244, 236};
inline const sf::Color kRowOdd{244, 237, 227};
inline const sf::Color kLeaderboardFirstRow{252, 237, 200};

struct MedalColors {
    sf::Color fill;
    sf::Color border;
};

// First, second and third place; flat fills keep the rank numbers readable.
inline const std::array<MedalColors, 3> kLeaderboardMedals{{
    {{232, 180, 73}, {245, 217, 145}},
    {{211, 214, 215}, {234, 235, 236}},
    {{207, 150, 101}, {235, 192, 156}},
}};

inline const sf::Color kModal{255, 251, 245};
inline const sf::Color kModalDim{0, 0, 0, 150};
inline const sf::Color kShadow{61, 44, 35, 24};
inline const sf::Color kButtonOutline{255, 251, 245};

inline const sf::Color kDangerButton{165, 45, 44};
inline const sf::Color kDangerButtonHover{178, 52, 50};
inline const sf::Color kSoundOn{52, 118, 62};
inline const sf::Color kSoundOnHover{62, 128, 72};
inline const sf::Color kSoundOff{101, 98, 91};
inline const sf::Color kSoundOffHover{114, 110, 102};
inline const sf::Color kOnStatusButton{255, 255, 255};
inline const sf::Color kToggleThumb{255, 251, 245};
inline const sf::Color kScoreIncrease{45, 107, 55};

inline const sf::Color kBoard{187, 173, 160};
inline const sf::Color kEmptyTile{205, 193, 180};
inline const sf::Color kTileText{119, 110, 101};
inline const sf::Color kOnTile{255, 255, 255};
inline const sf::Color kTileFallback{60, 58, 50};

struct TileColor {
    int value;
    sf::Color fill;
    sf::Color text;
};

inline const std::array<TileColor, 12> kTilePalette{{
    {2, {238, 228, 218}, kTextPrimary},
    {4, {237, 224, 200}, kTextPrimary},
    {8, {242, 177, 121}, kTextPrimary},
    {16, {245, 149, 99}, kTextPrimary},
    {32, {246, 124, 95}, kTextPrimary},
    {64, {246, 94, 59}, kOnTile},
    {128, {237, 207, 114}, kTextPrimary},
    {256, {237, 204, 97}, kTextPrimary},
    {512, {237, 200, 80}, kTextPrimary},
    {1024, {237, 197, 63}, kTextPrimary},
    {2048, {237, 194, 46}, kTextPrimary},
    {4096, {129, 168, 84}, kTextPrimary},
}};

} // namespace app::ui::theme::colors

namespace app::ui::theme::typography {

// Character sizes use SFML's unsigned-int API and logical view coordinates.
inline constexpr unsigned int kDisplaySize = 96U;
inline constexpr unsigned int kSubtitleSize = 22U;
inline constexpr unsigned int kLabelSize = 20U;
inline constexpr unsigned int kInputSize = 26U;
inline constexpr unsigned int kButtonSize = 22U;
inline constexpr unsigned int kLargeButtonSize = 32U;
inline constexpr unsigned int kCaptionSize = 16U;
inline constexpr unsigned int kHeadingSize = 46U;
inline constexpr unsigned int kModalHeadingSize = 40U;
inline constexpr unsigned int kCompactButtonSize = 18U;
inline constexpr unsigned int kTableHeaderSize = 19U;
inline constexpr unsigned int kLeaderboardMedalNumberSize = 18U;
inline constexpr unsigned int kScoreSize = 24U;
inline constexpr unsigned int kScoreCardCaptionSize = 20U;
inline constexpr unsigned int kScoreCardValueSize = 26U;
inline constexpr unsigned int kLeaderboardNameSize = 22U;
inline constexpr unsigned int kLeaderboardScoreSize = 22U;
inline constexpr unsigned int kLeaderboardRankSize = 20U;
inline constexpr unsigned int kWinScoreSize = 24U;
inline constexpr unsigned int kScoreFeedbackSize = 20U;
inline constexpr unsigned int kPlayingTitleSize = 64U;
inline constexpr unsigned int kPlayingDescriptionSize = 18U;
inline constexpr unsigned int kPlayingPlayerNameSize = 22U;
inline constexpr unsigned int kTileShortSize = 46U;
inline constexpr unsigned int kTileMediumSize = 38U;
inline constexpr unsigned int kTileLongSize = 32U;
inline constexpr unsigned int kTileExtraLongSize = 30U;
inline constexpr unsigned int kDecorationNumberSize = 40U;
inline constexpr unsigned int kDecorationLongNumberSize = 32U;

inline constexpr sf::Uint32 kDisplayStyle = sf::Text::Bold;
inline constexpr sf::Uint32 kHeadingStyle = sf::Text::Bold;
inline constexpr sf::Uint32 kBodyStyle = sf::Text::Regular;
inline constexpr sf::Uint32 kButtonStyle = sf::Text::Regular;
inline constexpr sf::Uint32 kEmphasisStyle = sf::Text::Bold;
inline constexpr sf::Uint32 kTileStyle = sf::Text::Bold;
inline constexpr sf::Uint32 kScoreCaptionStyle = sf::Text::Regular;
inline constexpr sf::Uint32 kScoreValueStyle = sf::Text::Bold;
inline constexpr sf::Uint32 kLeaderboardNameStyle = sf::Text::Regular;
inline constexpr sf::Uint32 kLeaderboardScoreStyle = sf::Text::Bold;
inline constexpr sf::Uint32 kScoreFeedbackStyle = sf::Text::Bold;

// Project readability floors in logical character-size equivalents, not physical
// pixels. Fitting reports a missed floor; it never overflows a reserved width.
namespace minimum {
inline constexpr float kInput = 16.f;
inline constexpr float kLabel = 18.f;
inline constexpr float kCaption = 16.f;
inline constexpr float kAction = 18.f;
inline constexpr float kScoreCardValue = 16.f;
inline constexpr float kLeaderboardScore = 18.f;
inline constexpr float kLeaderboardName = 22.f;
inline constexpr float kPlayerName = 22.f;
inline constexpr float kWinScore = 20.f;
inline constexpr float kScoreFeedback = 16.f;
inline constexpr float kTile = 16.f;
} // namespace minimum

} // namespace app::ui::theme::typography

namespace app::ui::theme::spacing {

inline constexpr float kXs = 4.f;
inline constexpr float kSm = 8.f;
inline constexpr float kMd = 12.f;
inline constexpr float kLg = 16.f;
inline constexpr float kXl = 24.f;
inline constexpr float kXxl = 32.f;

} // namespace app::ui::theme::spacing

namespace app::ui::theme::motion {

inline constexpr float kMergePopAmplitude = 0.17f;
inline constexpr float kScoreFeedbackDuration = 0.85f;
inline constexpr float kScoreFeedbackHoldFraction = 0.35f;
inline constexpr float kScoreFeedbackTravel = 22.f;

inline constexpr float kPanelRevealDuration = 0.18f;

inline constexpr float kButtonHoverDuration = 0.10f;
inline constexpr float kButtonPressDuration = 0.06f;
inline constexpr float kButtonReleaseDuration = 0.09f;
inline constexpr float kButtonPressedScale = 0.98f;

} // namespace app::ui::theme::motion

namespace app::ui::theme::radius {

inline constexpr float kButton = 16.f;
inline constexpr float kPanel = 12.f;
inline constexpr float kTile = 14.f;
inline constexpr float kBoard = spacing::kLg;
inline constexpr float kRow = 10.f;

} // namespace app::ui::theme::radius

namespace app::ui::theme::shadows {

inline const sf::Vector2f kOffset{0.f, 3.f};

} // namespace app::ui::theme::shadows

namespace app::ui::theme::sizes {

inline constexpr unsigned int kWindowWidth = 920U;
inline constexpr unsigned int kWindowHeight = 660U;
inline constexpr float kWindowMargin = spacing::kXxl;

inline constexpr std::size_t kCornerPointCount = 32U;
inline constexpr float kButtonBorder = 2.f;
inline constexpr float kButtonFocusBorder = 3.f;
inline constexpr float kButtonIconSize = 20.f;
inline constexpr float kHeadingIconSize = 40.f;
inline constexpr float kFormButtonHeight = 56.f;
inline constexpr float kDecorationSmall = 96.f;
inline constexpr float kDecorationMedium = 108.f;
inline constexpr float kDecorationLarge = 144.f;
inline constexpr float kPanelBorder = 2.f;
inline constexpr float kInputBorder = 2.f;
inline constexpr float kButtonHorizontalPadding = spacing::kXxl + spacing::kMd;
inline constexpr float kButtonVerticalPadding = spacing::kXl;
inline const sf::Vector2f kInput{320.f, 56.f};
inline const sf::Vector2f kToggle{44.f, 24.f};
inline constexpr float kLeaderboardRowHeight = 44.f;
inline constexpr float kLeaderboardRankWidth = 48.f;
inline constexpr float kLeaderboardMedalDiameter = 32.f;
inline constexpr float kLeaderboardMedalBorder = 2.f;
inline constexpr float kLeaderboardScoreWidth = 160.f;
inline constexpr float kLeaderboardWidth = 640.f;
inline constexpr float kLeaderboardHeaderHeight = 32.f;
inline const sf::Vector2f kLeaderboardBackButton{320.f, 56.f};

// Fixed playing columns share the board's 520 px outer extent.
inline constexpr float kPlayingBoardAreaExtent = 520.f;
inline constexpr float kPlayingColumnGap = spacing::kXl;
inline constexpr float kPlayingSidePanelWidth = 312.f;
inline constexpr float kPlayingDescriptionHeight = 48.f;
inline constexpr float kPlayingScoreCardHeight = 88.f;
// Numeric values get extra room inside existing cards; captions keep their inset.
inline constexpr float kScoreCardValueInset = spacing::kXs;
inline constexpr float kScoreFeedbackTextInset = 2.f;
inline constexpr float kPlayingActionHeight = 44.f;
inline constexpr float kPlayingSoundRowHeight = spacing::kXl;

// Four 110 px cells, three internal gaps and two outer insets fill 520 px.
inline constexpr int kCell = 110;
inline constexpr int kCellGap = static_cast<int>(spacing::kLg);
inline constexpr float kTileTextInset = 2.f;
inline constexpr int kTopPanelHeight = 80;
inline const sf::Vector2f kModal{390.f, 270.f};
inline const sf::Vector2f kWinModal{390.f, 400.f};
inline const sf::Vector2f kGameOverModal{390.f, 400.f};
inline constexpr float kGameOverScoreCardHeight = 88.f;

} // namespace app::ui::theme::sizes
