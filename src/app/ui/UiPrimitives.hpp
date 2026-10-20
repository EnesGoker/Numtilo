#pragma once

#include "app/ui/PointerActivation.hpp"
#include "app/ui/UiTheme.hpp"
#include "core/Board.hpp"

#include <SFML/Graphics.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string_view>

namespace app::ui {

inline constexpr int kCellSize = theme::sizes::kCell;
inline constexpr int kGridSize = numtilo_core::Board::kGridSize;
inline constexpr int kPadding = theme::sizes::kCellGap;
inline constexpr int kTopPanelHeight = theme::sizes::kTopPanelHeight;
inline constexpr std::size_t kRoundedCornerPointCount = theme::sizes::kCornerPointCount;
inline constexpr float kClickPadding = theme::spacing::kXs;

inline constexpr float kTileCornerRadius = theme::radius::kTile;

inline constexpr float kSlideAnimationDuration = 0.09f;
inline constexpr float kMergePopDuration = 0.12f;
inline constexpr float kSpawnFadeDuration = 0.14f;
inline constexpr float kFloatingScoreDuration = theme::motion::kScoreFeedbackDuration;

inline constexpr float kPi = 3.14159265358979323846f;

using Clock = std::chrono::steady_clock;

class RoundedRectShape final : public sf::Shape {
  public:
    RoundedRectShape(sf::Vector2f size = {}, float radius = 0.f,
                     std::size_t cornerPointCount = kRoundedCornerPointCount);

    void setSize(sf::Vector2f size);

    sf::Vector2f getSize() const;

    void setCornersRadius(float radius);

    float getCornersRadius() const;

    void setCornerPointCount(std::size_t cornerPointCount);

    std::size_t getPointCount() const override;

    sf::Vector2f getPoint(std::size_t index) const override;

  private:
    sf::Vector2f size_;
    float radius_{0.f};
    std::size_t cornerPointCount_{kRoundedCornerPointCount};
};

float clamp01(const float value);

sf::Uint8 toAlpha(const float normalized);

float lerp(const float a, const float b, const float t);

sf::Vector2f lerp(const sf::Vector2f &a, const sf::Vector2f &b, const float t);

bool isPrimaryMousePress(const sf::Event &event);

bool isPrimaryMouseRelease(const sf::Event &event);

// A shared local coordinate space, inset within the fixed application window.
sf::View createContentView();

bool containsViewPixel(const sf::RenderTarget &target, const sf::Vector2i &pixel);

sf::Vector2f cursorPosition(const sf::RenderWindow &window);

sf::Vector2f mouseButtonPosition(const sf::Event::MouseButtonEvent &event,
                                 const sf::RenderWindow &window);

bool containsWithPadding(const sf::FloatRect &bounds, const sf::Vector2f &point,
                         const float padding = kClickPadding);

// Use the normal outline for logical bounds, independently of focus styling.
sf::FloatRect controlBounds(const RoundedRectShape &control, float normalOutline);

sf::String toUnicode(const std::string_view utf8Text);

bool isNameWhitespace(sf::Uint32 codePoint);

struct PlayerNamePresentation {
    sf::String text;
    bool hasReplacements{false};
};

// A display-only projection; callers retain the original identity separately.
PlayerNamePresentation presentPlayerName(const sf::Font &font, const sf::String &name);

bool supportsText(const sf::Font &font, const sf::String &text);

sf::String localizedText(const sf::Font &font, const std::string_view preferredUtf8,
                         const std::string_view asciiFallback);

void centerTextOrigin(sf::Text &text);

struct TextFitResult {
    float effectiveSize;
    bool meetsMinimum;
};

// Bounds local horizontal ink for unrotated text and restores scale for short text.
// Height, rotation and external rendering transforms need separate caller checks. A missed
// readability floor is explicit; callers must not grow the drawing past its slot.
TextFitResult fitTextWidth(sf::Text &text, float width, float minimumSize = 0.f);

// Keeps display-only names on one line, with an ellipsis when needed.
void fitSingleLineText(sf::Text &text, float width);

sf::Vector2f cellCenter(const numtilo_core::Cell &cell);

// A scene explicitly starts this presentation; input resets do not restart it.
// Composite the whole group so overlapping shadows and text fade together.
class PanelReveal {
  public:
    void restart();

    void draw(sf::RenderTarget &target, const std::function<void(sf::RenderTarget &)> &drawContent);

  private:
    bool pending_{false};
    std::optional<Clock::time_point> startedAt_;
    sf::RenderTexture layer_;
};

enum class ButtonVariant { Primary, Secondary, Danger, SoundOn, SoundOff };
enum class ButtonIcon { Play, Trophy, Back, Restart, Home };
enum class PanelVariant { Standard, Side, Score, Modal };

struct ButtonColors {
    sf::Color fill;
    sf::Color hover;
    sf::Color text;
};

// Pointer feedback and activation share one gesture; scenes own focus/commands.
class ButtonFeedback {
  public:
    explicit ButtonFeedback(ButtonVariant variant);

    // Returns only pointer activation; scenes retain keyboard command ownership.
    bool handleEvent(const sf::Event &event, const sf::RenderWindow &window,
                     const RoundedRectShape &button, bool focused,
                     float hitPadding = kClickPadding);

    void setHovered(bool hovered);

    void cancelKeyboardPress();

    void reset();

    sf::Color fillColor() const;

    float scale() const;

  private:
    struct Transition {
        float from{0.f};
        float target{0.f};
        Clock::time_point startedAt{};
        float duration{0.f};

        float sample() const;

        void retarget(float value, float seconds);
    };

    void updatePressed();

    ButtonVariant variant_;
    Transition hover_;
    Transition press_;
    PointerActivation pointer_;
    std::optional<sf::Keyboard::Key> pressedKey_;
};

ButtonColors buttonColors(ButtonVariant variant);

void styleText(sf::Text &text, const sf::Color &color,
               sf::Uint32 style = theme::typography::kBodyStyle);

void styleButton(RoundedRectShape &button, ButtonVariant variant);

void styleButton(RoundedRectShape &button, sf::Text &label, ButtonVariant variant);

RoundedRectShape createButtonForText(const sf::Text &label, ButtonVariant variant);

void updateButtonHover(RoundedRectShape &button, bool hovered, ButtonVariant variant);

void updateButtonHover(RoundedRectShape &button, bool hovered, ButtonVariant variant,
                       ButtonFeedback &feedback);

void updateButtonFocus(RoundedRectShape &button, bool focused);

void updateButtonFocus(RoundedRectShape &button, bool focused, ButtonFeedback &feedback);

void drawButton(sf::RenderTarget &target, const RoundedRectShape &button,
                const sf::RenderStates &states = {});

TextFitResult drawButton(sf::RenderTarget &target, const RoundedRectShape &button,
                         const sf::Text &label, const sf::RenderStates &states = {});

TextFitResult drawButton(sf::RenderTarget &target, const RoundedRectShape &button,
                         const sf::Text &label, ButtonIcon icon,
                         const sf::RenderStates &states = {});

TextFitResult drawButton(sf::RenderTarget &target, const RoundedRectShape &button,
                         const sf::Text &label, const ButtonFeedback &feedback);

TextFitResult drawButton(sf::RenderTarget &target, const RoundedRectShape &button,
                         const sf::Text &label, ButtonIcon icon, const ButtonFeedback &feedback);

void drawTrophyIcon(sf::RenderTarget &target, sf::Vector2f topLeft, float size, sf::Color color,
                    const sf::RenderStates &states = {});

void stylePanel(RoundedRectShape &panel, PanelVariant variant = PanelVariant::Standard);

RoundedRectShape createPanel(sf::Vector2f size, PanelVariant variant = PanelVariant::Standard);

void drawPanel(sf::RenderTarget &target, const RoundedRectShape &panel);

RoundedRectShape createTextInput(sf::Vector2f size = theme::sizes::kInput);

void updateTextInputStyle(RoundedRectShape &input, bool focused, bool invalid = false);

TextFitResult drawTextInput(sf::RenderTarget &target, const RoundedRectShape &input,
                            const sf::Text &text);

struct ScoreCardFitResult {
    TextFitResult caption;
    TextFitResult value;
};

ScoreCardFitResult drawScoreCard(sf::RenderTarget &target, const sf::Font &font,
                                 sf::FloatRect bounds, const sf::String &label,
                                 const sf::String &value);

void drawToggle(sf::RenderTarget &target, sf::Vector2f position, bool enabled,
                bool hovered = false);

void drawLeaderboardHeader(sf::RenderTarget &target, const sf::Font &font, sf::FloatRect bounds,
                           const sf::String &nameLabel, const sf::String &scoreLabel);

void drawLeaderboardRow(sf::RenderTarget &target, const sf::Font &font, sf::FloatRect bounds,
                        std::size_t rank, const sf::String &name, const sf::String &score);

sf::RectangleShape createModalOverlay(sf::Vector2f size);

void drawModal(sf::RenderTarget &target, const sf::RectangleShape &overlay,
               const RoundedRectShape &panel);

sf::Color getTileColor(const int value);

sf::Color getTileTextColor(int value);

void drawEmptyCell(sf::RenderTarget &window, const sf::Vector2f &center, const float scale = 1.f);

TextFitResult drawTile(sf::RenderTarget &window, const sf::Font &font, const int value,
                       const sf::Vector2f &center, const float scale = 1.f,
                       const sf::Uint8 alpha = 255);

std::optional<numtilo_core::Direction> mapDirection(const sf::Keyboard::Key key);

} // namespace app::ui
