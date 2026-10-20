#include "app/ui/UiPrimitives.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace app::ui {

void PanelReveal::restart() {
    pending_ = true;
    startedAt_.reset();
}

void PanelReveal::draw(sf::RenderTarget &target,
                       const std::function<void(sf::RenderTarget &)> &drawContent) {
    const auto now = Clock::now();
    const bool starting = pending_;
    if (pending_) {
        startedAt_ = now;
        pending_ = false;
    }
    const float progress = startedAt_.has_value()
                               ? clamp01(std::chrono::duration<float>(now - *startedAt_).count() /
                                         theme::motion::kPanelRevealDuration)
                               : 1.f;
    if (progress >= 1.f) {
        startedAt_.reset();
        drawContent(target);
        return;
    }
    const auto size = target.getSize();
    if (layer_.getSize() != size && !layer_.create(size.x, size.y)) {
        startedAt_.reset();
        drawContent(target);
        return;
    }
    const auto view = target.getView();
    layer_.setView(view);
    layer_.clear(sf::Color::Transparent);
    drawContent(layer_);
    layer_.display();
    if (starting) {
        // First-use GPU allocation/rendering must not consume the entry duration.
        startedAt_ = Clock::now();
    }

    const auto alpha = toAlpha(progress * progress * (3.f - 2.f * progress));
    sf::Sprite content(layer_.getTexture());
    // RenderTexture already contains premultiplied RGB. Attenuate both RGB and
    // alpha, then composite once to preserve text edges and overlapping layers.
    content.setColor({alpha, alpha, alpha, alpha});
    const auto &fullView = target.getDefaultView();
    content.setPosition(fullView.getCenter() - fullView.getSize() / 2.f);
    content.setScale(fullView.getSize().x / static_cast<float>(size.x),
                     fullView.getSize().y / static_cast<float>(size.y));
    target.setView(fullView);
    target.draw(content, sf::BlendMode(sf::BlendMode::One, sf::BlendMode::OneMinusSrcAlpha));
    target.setView(view);
}

RoundedRectShape::RoundedRectShape(sf::Vector2f size, float radius, std::size_t cornerPointCount)
    : size_(size), radius_(radius), cornerPointCount_(std::max<std::size_t>(cornerPointCount, 2U)) {
    update();
}

void RoundedRectShape::setSize(sf::Vector2f size) {
    size_ = size;
    update();
}

sf::Vector2f RoundedRectShape::getSize() const {
    return size_;
}

void RoundedRectShape::setCornersRadius(float radius) {
    radius_ = radius;
    update();
}

float RoundedRectShape::getCornersRadius() const {
    const float maxRadius = std::min(size_.x, size_.y) * 0.5f;
    return std::clamp(radius_, 0.f, maxRadius);
}

void RoundedRectShape::setCornerPointCount(std::size_t cornerPointCount) {
    cornerPointCount_ = std::max<std::size_t>(cornerPointCount, 2U);
    update();
}

std::size_t RoundedRectShape::getPointCount() const {
    return cornerPointCount_ * 4U;
}

sf::Vector2f RoundedRectShape::getPoint(std::size_t index) const {
    const float radius = getCornersRadius();

    if (radius <= 0.f) {
        switch ((index / cornerPointCount_) % 4U) {
        case 0:
            return {0.f, 0.f};
        case 1:
            return {size_.x, 0.f};
        case 2:
            return {size_.x, size_.y};
        default:
            return {0.f, size_.y};
        }
    }

    const std::array<sf::Vector2f, 4> centers = {
        sf::Vector2f(radius, radius),
        sf::Vector2f(size_.x - radius, radius),
        sf::Vector2f(size_.x - radius, size_.y - radius),
        sf::Vector2f(radius, size_.y - radius),
    };

    const std::array<float, 4> baseAngles = {180.f, 270.f, 0.f, 90.f};

    const std::size_t corner = (index / cornerPointCount_) % 4U;
    const std::size_t pointInCorner = index % cornerPointCount_;
    const float step = 90.f / static_cast<float>(cornerPointCount_ - 1U);
    const float angle =
        (baseAngles[corner] + static_cast<float>(pointInCorner) * step) * (kPi / 180.f);

    return {
        centers[corner].x + std::cos(angle) * radius,
        centers[corner].y + std::sin(angle) * radius,
    };
}

float clamp01(const float value) {
    return std::clamp(value, 0.f, 1.f);
}

sf::Uint8 toAlpha(const float normalized) {
    return static_cast<sf::Uint8>(std::clamp(normalized, 0.f, 1.f) * 255.f);
}

float lerp(const float a, const float b, const float t) {
    return a + (b - a) * t;
}

sf::Vector2f lerp(const sf::Vector2f &a, const sf::Vector2f &b, const float t) {
    return {lerp(a.x, b.x, t), lerp(a.y, b.y, t)};
}

bool isPrimaryMousePress(const sf::Event &event) {
    return event.type == sf::Event::MouseButtonPressed &&
           event.mouseButton.button == sf::Mouse::Left;
}

bool isPrimaryMouseRelease(const sf::Event &event) {
    return event.type == sf::Event::MouseButtonReleased &&
           event.mouseButton.button == sf::Mouse::Left;
}

sf::View createContentView() {
    constexpr float width = static_cast<float>(theme::sizes::kWindowWidth);
    constexpr float height = static_cast<float>(theme::sizes::kWindowHeight);
    constexpr float margin = theme::sizes::kWindowMargin;
    static_assert(width > 2.f * margin && height > 2.f * margin);
    sf::View view(sf::FloatRect(0.f, 0.f, width - 2.f * margin, height - 2.f * margin));
    view.setViewport({margin / width, margin / height, (width - 2.f * margin) / width,
                      (height - 2.f * margin) / height});
    return view;
}

bool containsViewPixel(const sf::RenderTarget &target, const sf::Vector2i &pixel) {
    return target.getViewport(target.getView()).contains(pixel);
}

sf::Vector2f cursorPosition(const sf::RenderWindow &window) {
    return window.mapPixelToCoords(sf::Mouse::getPosition(window));
}

sf::Vector2f mouseButtonPosition(const sf::Event::MouseButtonEvent &event,
                                 const sf::RenderWindow &window) {
    return window.mapPixelToCoords({event.x, event.y});
}

bool containsWithPadding(const sf::FloatRect &bounds, const sf::Vector2f &point,
                         const float padding) {
    return sf::FloatRect(bounds.left - padding, bounds.top - padding, bounds.width + 2.f * padding,
                         bounds.height + 2.f * padding)
        .contains(point);
}

sf::FloatRect controlBounds(const RoundedRectShape &control, const float normalOutline) {
    if (control.getOutlineThickness() == normalOutline) {
        return control.getGlobalBounds();
    }
    // Preserve SFML's rounded-outline extrema and transform without changing the drawing.
    auto logicalControl = control;
    logicalControl.setOutlineThickness(normalOutline);
    return logicalControl.getGlobalBounds();
}

sf::String toUnicode(const std::string_view utf8Text) {
    return sf::String::fromUtf8(utf8Text.begin(), utf8Text.end());
}

bool isNameWhitespace(const sf::Uint32 codePoint) {
    return (codePoint >= 0x09U && codePoint <= 0x0DU) || codePoint == 0x20U || codePoint == 0x85U ||
           codePoint == 0xA0U || codePoint == 0x1680U ||
           (codePoint >= 0x2000U && codePoint <= 0x200AU) || codePoint == 0x2028U ||
           codePoint == 0x2029U || codePoint == 0x202FU || codePoint == 0x205FU ||
           codePoint == 0x3000U;
}

PlayerNamePresentation presentPlayerName(const sf::Font &font, const sf::String &name) {
    PlayerNamePresentation result;
    bool visible = false;
    for (const auto codePoint : name) {
        if (isNameWhitespace(codePoint)) {
            result.text += U' ';
            continue;
        }
        if (font.hasGlyph(codePoint)) {
            const auto &glyph = font.getGlyph(codePoint, theme::typography::kInputSize, false);
            if (glyph.bounds.width > 0.f && glyph.bounds.height > 0.f) {
                result.text += codePoint;
                visible = true;
                continue;
            }
        }
        result.text += U'?';
        result.hasReplacements = true;
        visible = true;
    }
    if (!visible) {
        result.text = "?";
        result.hasReplacements = true;
    }
    return result;
}

bool supportsText(const sf::Font &font, const sf::String &text) {
    for (std::size_t i = 0; i < text.getSize(); ++i) {
        const sf::Uint32 codePoint = text[i];
        if (codePoint == U' ' || codePoint == U'\n' || codePoint == U'\t' || codePoint == U'\r') {
            continue;
        }
        if (!font.hasGlyph(codePoint)) {
            return false;
        }
    }
    return true;
}

sf::String localizedText(const sf::Font &font, const std::string_view preferredUtf8,
                         const std::string_view asciiFallback) {
    const sf::String preferred = toUnicode(preferredUtf8);
    if (supportsText(font, preferred)) {
        return preferred;
    }
    return toUnicode(asciiFallback);
}

void centerTextOrigin(sf::Text &text) {
    const auto bounds = text.getLocalBounds();
    text.setOrigin(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
}

sf::Vector2f cellCenter(const numtilo_core::Cell &cell) {
    return {
        static_cast<float>(cell.col * kCellSize + (cell.col + 1) * kPadding + kCellSize / 2),
        static_cast<float>(kTopPanelHeight + cell.row * kCellSize + (cell.row + 1) * kPadding +
                           kCellSize / 2),
    };
}

TextFitResult fitTextWidth(sf::Text &text, const float width, const float minimumSize) {
    const float naturalWidth = text.getLocalBounds().width;
    const float scale =
        naturalWidth > 0.f ? std::min(1.f, std::max(0.f, width) / naturalWidth) : 1.f;
    text.setScale(scale, scale);
    const float effectiveSize = static_cast<float>(text.getCharacterSize()) * scale;
    const bool empty = text.getString().isEmpty();
    return {effectiveSize,
            empty || (text.getFont() != nullptr && effectiveSize >= std::max(0.f, minimumSize))};
}

namespace {

void drawShadow(sf::RenderTarget &target, const RoundedRectShape &shape,
                const sf::RenderStates &states = {}) {
    auto shadow = shape;
    shadow.setOutlineThickness(0.f);
    shadow.setFillColor(theme::colors::kShadow);
    shadow.move(theme::shadows::kOffset);
    target.draw(shadow, states);
}

struct LeaderboardColumns {
    float rankCenter;
    float nameLeft;
    float nameWidth;
    float scoreRight;
};

sf::String singleLineText(sf::String text) {
    for (auto &codePoint : text) {
        if (codePoint < 32U || codePoint == 127U || codePoint == 0x85U || codePoint == 0x2028U ||
            codePoint == 0x2029U) {
            codePoint = U' ';
        }
    }
    return text;
}

void ellipsizeText(sf::Text &text, const float requestedWidth) {
    const float width = std::max(0.f, requestedWidth);
    if (text.getString().isEmpty() || text.getFont() == nullptr) {
        return;
    }
    if (text.getLocalBounds().width <= width) {
        return;
    }
    const sf::String original = text.getString();
    const sf::String suffix = localizedText(*text.getFont(), "…", "...");
    text.setString(suffix);
    if (text.getLocalBounds().width > width) {
        text.setString("");
        return;
    }
    std::size_t low = 0U;
    std::size_t high = original.getSize();
    while (low < high) {
        const std::size_t middle = low + (high - low + 1U) / 2U;
        text.setString(original.substring(0U, middle) + suffix);
        if (text.getLocalBounds().width <= width) {
            low = middle;
        } else {
            high = middle - 1U;
        }
    }
    text.setString(original.substring(0U, low) + suffix);
}

LeaderboardColumns leaderboardColumns(const sf::FloatRect bounds) {
    const float padding = theme::spacing::kMd;
    const float nameLeft = bounds.left + padding + theme::sizes::kLeaderboardRankWidth;
    const float scoreRight = bounds.left + bounds.width - padding;
    return {bounds.left + padding + theme::sizes::kLeaderboardRankWidth / 2.f, nameLeft,
            scoreRight - theme::sizes::kLeaderboardScoreWidth - padding - nameLeft, scoreRight};
}

} // namespace

void fitSingleLineText(sf::Text &text, const float width) {
    text.setScale(1.f, 1.f);
    text.setString(singleLineText(text.getString()));
    ellipsizeText(text, width);
}

ButtonFeedback::ButtonFeedback(const ButtonVariant variant) : variant_(variant) {
}

float ButtonFeedback::Transition::sample() const {
    if (from == target || duration <= 0.f) {
        return target;
    }
    const float elapsed = std::chrono::duration<float>(Clock::now() - startedAt).count();
    const float t = clamp01(elapsed / duration);
    return lerp(from, target, t * t * (3.f - 2.f * t));
}

void ButtonFeedback::Transition::retarget(const float value, const float seconds) {
    if (target == value) {
        return;
    }
    from = sample();
    target = value;
    startedAt = Clock::now();
    duration = seconds;
}

bool ButtonFeedback::handleEvent(const sf::Event &event, const sf::RenderWindow &window,
                                 const RoundedRectShape &button, const bool focused,
                                 const float hitPadding) {
    if (event.type == sf::Event::LostFocus) {
        reset();
        return false;
    }
    const bool activated = pointer_.handleEvent(event, [&](const sf::Vector2i &pixel) {
        return containsWithPadding(controlBounds(button, theme::sizes::kButtonBorder),
                                   window.mapPixelToCoords(pixel), hitPadding);
    });
    if (isPrimaryMousePress(event)) {
        pressedKey_.reset();
    } else if (event.type == sf::Event::KeyPressed && focused && !event.key.alt &&
               !event.key.control && !event.key.system &&
               (event.key.code == sf::Keyboard::Enter || event.key.code == sf::Keyboard::Space)) {
        pressedKey_ = event.key.code;
    } else if (event.type == sf::Event::KeyReleased && pressedKey_ == event.key.code) {
        pressedKey_.reset();
    }
    updatePressed();
    return activated;
}

void ButtonFeedback::setHovered(const bool hovered) {
    hover_.retarget(hovered ? 1.f : 0.f, theme::motion::kButtonHoverDuration);
}

void ButtonFeedback::cancelKeyboardPress() {
    pressedKey_.reset();
    updatePressed();
}

void ButtonFeedback::reset() {
    hover_ = {};
    press_ = {};
    pointer_.reset();
    pressedKey_.reset();
}

void ButtonFeedback::updatePressed() {
    const bool pressed = pointer_.pressed() || pressedKey_.has_value();
    press_.retarget(pressed ? 1.f : 0.f, pressed ? theme::motion::kButtonPressDuration
                                                 : theme::motion::kButtonReleaseDuration);
}

sf::Color ButtonFeedback::fillColor() const {
    const auto colors = buttonColors(variant_);
    const float progress = hover_.sample();
    const auto channel = [progress](const sf::Uint8 from, const sf::Uint8 to) {
        return static_cast<sf::Uint8>(std::round(lerp(from, to, progress)));
    };
    return {channel(colors.fill.r, colors.hover.r), channel(colors.fill.g, colors.hover.g),
            channel(colors.fill.b, colors.hover.b), channel(colors.fill.a, colors.hover.a)};
}

float ButtonFeedback::scale() const {
    return lerp(1.f, theme::motion::kButtonPressedScale, press_.sample());
}

ButtonColors buttonColors(const ButtonVariant variant) {
    using namespace theme::colors;
    switch (variant) {
    case ButtonVariant::Secondary:
        return {kSecondaryButton, kSecondaryButtonHover, kOnSecondaryButton};
    case ButtonVariant::Danger:
        return {kDangerButton, kDangerButtonHover, kOnStatusButton};
    case ButtonVariant::SoundOn:
        return {kSoundOn, kSoundOnHover, kOnStatusButton};
    case ButtonVariant::SoundOff:
        return {kSoundOff, kSoundOffHover, kOnStatusButton};
    case ButtonVariant::Primary:
        return {kPrimaryButton, kPrimaryButtonHover, kOnPrimaryButton};
    }
    return {kPrimaryButton, kPrimaryButtonHover, kOnPrimaryButton};
}

void styleText(sf::Text &text, const sf::Color &color, const sf::Uint32 style) {
    text.setFillColor(color);
    text.setStyle(style);
}

void styleButton(RoundedRectShape &button, const ButtonVariant variant) {
    button.setCornersRadius(theme::radius::kButton);
    button.setFillColor(buttonColors(variant).fill);
    button.setOutlineThickness(theme::sizes::kButtonBorder);
    button.setOutlineColor(theme::colors::kButtonOutline);
}

void styleButton(RoundedRectShape &button, sf::Text &label, const ButtonVariant variant) {
    styleButton(button, variant);
    styleText(label, buttonColors(variant).text, theme::typography::kButtonStyle);
}

RoundedRectShape createButtonForText(const sf::Text &label, const ButtonVariant variant) {
    auto styledLabel = label;
    styledLabel.setStyle(theme::typography::kButtonStyle);
    const auto bounds = styledLabel.getLocalBounds();
    RoundedRectShape button({bounds.width + theme::sizes::kButtonHorizontalPadding,
                             bounds.height + theme::sizes::kButtonVerticalPadding});
    styleButton(button, variant);
    button.setOrigin(button.getSize() / 2.f);
    return button;
}

void updateButtonHover(RoundedRectShape &button, const bool hovered, const ButtonVariant variant) {
    const auto colors = buttonColors(variant);
    button.setFillColor(hovered ? colors.hover : colors.fill);
}

void updateButtonHover(RoundedRectShape &button, const bool hovered, const ButtonVariant variant,
                       ButtonFeedback &feedback) {
    updateButtonHover(button, hovered, variant);
    feedback.setHovered(hovered);
}

void updateButtonFocus(RoundedRectShape &button, const bool focused) {
    button.setOutlineColor(focused ? theme::colors::kInputFocusBorder
                                   : theme::colors::kButtonOutline);
    button.setOutlineThickness(focused ? theme::sizes::kButtonFocusBorder
                                       : theme::sizes::kButtonBorder);
}

void updateButtonFocus(RoundedRectShape &button, const bool focused, ButtonFeedback &feedback) {
    updateButtonFocus(button, focused);
    if (!focused) {
        feedback.cancelKeyboardPress();
    }
}

namespace {

sf::RenderStates buttonFeedbackStates(const RoundedRectShape &button, const float scale) {
    const auto bounds = button.getGlobalBounds();
    const sf::Vector2f center(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
    sf::RenderStates states;
    states.transform.translate(center).scale(scale, scale).translate(-center);
    return states;
}

} // namespace

TextFitResult drawButton(sf::RenderTarget &target, const RoundedRectShape &button,
                         const sf::Text &label, const ButtonFeedback &feedback) {
    auto visual = button;
    visual.setFillColor(feedback.fillColor());
    const float scale = feedback.scale();
    auto fit = drawButton(target, visual, label, buttonFeedbackStates(button, scale));
    fit.effectiveSize *= scale;
    fit.meetsMinimum = fit.meetsMinimum && fit.effectiveSize >= theme::typography::minimum::kAction;
    return fit;
}

TextFitResult drawButton(sf::RenderTarget &target, const RoundedRectShape &button,
                         const sf::Text &label, const ButtonIcon icon,
                         const ButtonFeedback &feedback) {
    auto visual = button;
    visual.setFillColor(feedback.fillColor());
    const float scale = feedback.scale();
    auto fit = drawButton(target, visual, label, icon, buttonFeedbackStates(button, scale));
    fit.effectiveSize *= scale;
    fit.meetsMinimum = fit.meetsMinimum && fit.effectiveSize >= theme::typography::minimum::kAction;
    return fit;
}

void drawButton(sf::RenderTarget &target, const RoundedRectShape &button,
                const sf::RenderStates &states) {
    drawShadow(target, button, states);
    target.draw(button, states);
}

TextFitResult drawButton(sf::RenderTarget &target, const RoundedRectShape &button,
                         const sf::Text &label, const sf::RenderStates &states) {
    drawButton(target, button, states);
    auto fittedLabel = label;
    const auto fit =
        fitTextWidth(fittedLabel, button.getSize().x - theme::sizes::kButtonHorizontalPadding,
                     theme::typography::minimum::kAction);
    centerTextOrigin(fittedLabel);
    const auto bounds = button.getGlobalBounds();
    fittedLabel.setPosition(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
    target.draw(fittedLabel, states);
    return fit;
}

TextFitResult drawButton(sf::RenderTarget &target, const RoundedRectShape &button,
                         const sf::Text &label, const ButtonIcon icon,
                         const sf::RenderStates &states) {
    drawButton(target, button, states);
    const float size = theme::sizes::kButtonIconSize;
    const float gap = theme::spacing::kSm;
    auto fittedLabel = label;
    const auto fit = fitTextWidth(
        fittedLabel, button.getSize().x - theme::sizes::kButtonHorizontalPadding - size - gap,
        theme::typography::minimum::kAction);
    centerTextOrigin(fittedLabel);
    const float groupWidth =
        size + gap + fittedLabel.getLocalBounds().width * fittedLabel.getScale().x;
    const auto bounds = button.getGlobalBounds();
    const sf::Vector2f center(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
    const sf::Vector2f iconTopLeft(center.x - groupWidth / 2.f, center.y - size / 2.f);
    fittedLabel.setPosition(center.x + (size + gap) / 2.f, center.y);

    if (icon == ButtonIcon::Play) {
        sf::ConvexShape triangle(3U);
        triangle.setPoint(0U, {0.f, 0.f});
        triangle.setPoint(1U, {size, size / 2.f});
        triangle.setPoint(2U, {0.f, size});
        triangle.setFillColor(label.getFillColor());
        triangle.setPosition(iconTopLeft);
        target.draw(triangle, states);
    } else if (icon == ButtonIcon::Trophy) {
        drawTrophyIcon(target, iconTopLeft, size, label.getFillColor(), states);
    } else if (icon == ButtonIcon::Back) {
        const float stroke = size * 0.1f;
        const sf::Vector2f tip(iconTopLeft.x + size * 0.1f, iconTopLeft.y + size / 2.f);
        sf::RectangleShape shaft({size * 0.8f, stroke});
        shaft.setOrigin(0.f, stroke / 2.f);
        shaft.setPosition(tip);
        shaft.setFillColor(label.getFillColor());
        target.draw(shaft, states);
        sf::RectangleShape chevron({size * 0.5f, stroke});
        chevron.setOrigin(0.f, stroke / 2.f);
        chevron.setPosition(tip);
        chevron.setFillColor(label.getFillColor());
        chevron.setRotation(-45.f);
        target.draw(chevron, states);
        chevron.setRotation(45.f);
        target.draw(chevron, states);
    } else if (icon == ButtonIcon::Restart) {
        const sf::Vector2f arcCenter = iconTopLeft + sf::Vector2f(size / 2.f, size / 2.f);
        const float radius = size * 0.36f;
        const float halfStroke = size * 0.055f;
        sf::VertexArray arc(sf::TriangleStrip);
        constexpr unsigned int segments = 32U;
        for (unsigned int i = 0U; i <= segments; ++i) {
            const float angle = (-80.f + 290.f * static_cast<float>(i) / segments) * kPi / 180.f;
            const sf::Vector2f radial(std::cos(angle), std::sin(angle));
            arc.append({arcCenter + radial * (radius - halfStroke), label.getFillColor()});
            arc.append({arcCenter + radial * (radius + halfStroke), label.getFillColor()});
        }
        target.draw(arc, states);
        const float endAngle = 210.f * kPi / 180.f;
        const sf::Vector2f radial(std::cos(endAngle), std::sin(endAngle));
        const sf::Vector2f tangent(-radial.y, radial.x);
        const auto tip = arcCenter + radial * radius;
        sf::ConvexShape arrow(3U);
        arrow.setPoint(0U, tip + tangent * (size * 0.17f));
        arrow.setPoint(1U, tip - tangent * (size * 0.08f) + radial * (size * 0.15f));
        arrow.setPoint(2U, tip - tangent * (size * 0.08f) - radial * (size * 0.15f));
        arrow.setFillColor(label.getFillColor());
        target.draw(arrow, states);
    } else if (icon == ButtonIcon::Home) {
        sf::ConvexShape roof(3U);
        roof.setPoint(0U, {0.f, size * 0.45f});
        roof.setPoint(1U, {size * 0.5f, size * 0.05f});
        roof.setPoint(2U, {size, size * 0.45f});
        roof.setPosition(iconTopLeft);
        roof.setFillColor(label.getFillColor());
        target.draw(roof, states);
        for (const auto bounds :
             {sf::FloatRect(0.18f, 0.42f, 0.64f, 0.2f), sf::FloatRect(0.18f, 0.62f, 0.24f, 0.3f),
              sf::FloatRect(0.58f, 0.62f, 0.24f, 0.3f)}) {
            sf::RectangleShape body({bounds.width * size, bounds.height * size});
            body.setPosition(iconTopLeft + sf::Vector2f(bounds.left * size, bounds.top * size));
            body.setFillColor(label.getFillColor());
            target.draw(body, states);
        }
    }
    target.draw(fittedLabel, states);
    return fit;
}

void drawTrophyIcon(sf::RenderTarget &target, const sf::Vector2f topLeft, const float size,
                    const sf::Color color, const sf::RenderStates &states) {
    sf::CircleShape handle(size * 0.2f);
    handle.setFillColor(sf::Color::Transparent);
    handle.setOutlineColor(color);
    handle.setOutlineThickness(size * 0.1f);
    handle.setPosition(topLeft.x + size * 0.05f, topLeft.y + size * 0.05f);
    target.draw(handle, states);
    handle.setPosition(topLeft.x + size * 0.55f, topLeft.y + size * 0.05f);
    target.draw(handle, states);
    sf::ConvexShape bowl(4U);
    bowl.setPoint(0U, {size * 0.2f, 0.f});
    bowl.setPoint(1U, {size * 0.8f, 0.f});
    bowl.setPoint(2U, {size * 0.65f, size * 0.55f});
    bowl.setPoint(3U, {size * 0.35f, size * 0.55f});
    bowl.setFillColor(color);
    bowl.setPosition(topLeft);
    target.draw(bowl, states);
    sf::RectangleShape stem({size * 0.15f, size * 0.3f});
    stem.setFillColor(color);
    stem.setPosition(topLeft.x + size * 0.425f, topLeft.y + size * 0.5f);
    target.draw(stem, states);
    sf::RectangleShape base({size * 0.6f, size * 0.15f});
    base.setFillColor(color);
    base.setPosition(topLeft.x + size * 0.2f, topLeft.y + size * 0.8f);
    target.draw(base, states);
}

void stylePanel(RoundedRectShape &panel, const PanelVariant variant) {
    auto fill = theme::colors::kPanel;
    switch (variant) {
    case PanelVariant::Side:
        fill = theme::colors::kSidePanel;
        break;
    case PanelVariant::Score:
        fill = theme::colors::kScoreCard;
        break;
    case PanelVariant::Modal:
        fill = theme::colors::kModal;
        break;
    case PanelVariant::Standard:
        break;
    }
    panel.setCornersRadius(theme::radius::kPanel);
    panel.setFillColor(fill);
    panel.setOutlineThickness(theme::sizes::kPanelBorder);
    panel.setOutlineColor(theme::colors::kPanelBorder);
}

RoundedRectShape createPanel(const sf::Vector2f size, const PanelVariant variant) {
    RoundedRectShape panel(size);
    stylePanel(panel, variant);
    return panel;
}

void drawPanel(sf::RenderTarget &target, const RoundedRectShape &panel) {
    drawShadow(target, panel);
    target.draw(panel);
}

RoundedRectShape createTextInput(const sf::Vector2f size) {
    RoundedRectShape input(size, theme::radius::kButton);
    input.setFillColor(theme::colors::kInputBackground);
    input.setOutlineThickness(theme::sizes::kInputBorder);
    updateTextInputStyle(input, false);
    return input;
}

void updateTextInputStyle(RoundedRectShape &input, const bool focused, const bool invalid) {
    input.setOutlineColor(focused
                              ? theme::colors::kInputFocusBorder
                              : (invalid ? theme::colors::kError : theme::colors::kInputBorder));
}

TextFitResult drawTextInput(sf::RenderTarget &target, const RoundedRectShape &input,
                            const sf::Text &text) {
    target.draw(input);
    auto fittedText = text;
    const auto fit = fitTextWidth(fittedText, input.getSize().x - theme::spacing::kXl,
                                  theme::typography::minimum::kInput);
    centerTextOrigin(fittedText);
    const auto bounds = input.getGlobalBounds();
    fittedText.setPosition(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
    target.draw(fittedText);
    return fit;
}

ScoreCardFitResult drawScoreCard(sf::RenderTarget &target, const sf::Font &font,
                                 const sf::FloatRect bounds, const sf::String &label,
                                 const sf::String &value) {
    auto panel = createPanel({bounds.width, bounds.height}, PanelVariant::Score);
    panel.setPosition(bounds.left, bounds.top);
    drawPanel(target, panel);
    sf::Text caption(label, font, theme::typography::kScoreCardCaptionSize);
    styleText(caption, theme::colors::kTextSecondary, theme::typography::kScoreCaptionStyle);
    const auto captionFit = fitTextWidth(caption, bounds.width - theme::spacing::kXl,
                                         theme::typography::minimum::kLabel);
    centerTextOrigin(caption);
    caption.setPosition(bounds.left + bounds.width * 0.5f, bounds.top + bounds.height * 0.3f);
    target.draw(caption);
    sf::Text number(value, font, theme::typography::kScoreCardValueSize);
    styleText(number, theme::colors::kTextPrimary, theme::typography::kScoreValueStyle);
    const auto valueFit =
        fitTextWidth(number, bounds.width - 2.f * theme::sizes::kScoreCardValueInset,
                     theme::typography::minimum::kScoreCardValue);
    centerTextOrigin(number);
    number.setPosition(bounds.left + bounds.width * 0.5f, bounds.top + bounds.height * 0.7f);
    target.draw(number);
    return {captionFit, valueFit};
}

void drawToggle(sf::RenderTarget &target, const sf::Vector2f position, const bool enabled,
                const bool hovered) {
    const auto size = theme::sizes::kToggle;
    RoundedRectShape track(size, size.y * 0.5f);
    track.setPosition(position);
    const auto colors = buttonColors(enabled ? ButtonVariant::SoundOn : ButtonVariant::SoundOff);
    track.setFillColor(hovered ? colors.hover : colors.fill);
    target.draw(track);
    const float padding = theme::spacing::kXs;
    sf::CircleShape thumb(size.y * 0.5f - padding);
    thumb.setFillColor(theme::colors::kToggleThumb);
    thumb.setPosition(position.x + (enabled ? size.x - size.y + padding : padding),
                      position.y + padding);
    target.draw(thumb);
}

void drawLeaderboardHeader(sf::RenderTarget &target, const sf::Font &font,
                           const sf::FloatRect bounds, const sf::String &nameLabel,
                           const sf::String &scoreLabel) {
    const auto columns = leaderboardColumns(bounds);
    const float centerY = bounds.top + bounds.height / 2.f;
    sf::Text rankText("#", font, theme::typography::kTableHeaderSize);
    styleText(rankText, theme::colors::kTextSecondary, theme::typography::kEmphasisStyle);
    centerTextOrigin(rankText);
    rankText.setPosition(columns.rankCenter, centerY);
    target.draw(rankText);

    sf::Text nameText(nameLabel, font, theme::typography::kTableHeaderSize);
    styleText(nameText, theme::colors::kTextSecondary, theme::typography::kEmphasisStyle);
    centerTextOrigin(nameText);
    nameText.setOrigin(nameText.getLocalBounds().left, nameText.getOrigin().y);
    fitTextWidth(nameText, columns.nameWidth);
    nameText.setPosition(columns.nameLeft, centerY);
    target.draw(nameText);

    sf::Text scoreText(scoreLabel, font, theme::typography::kTableHeaderSize);
    styleText(scoreText, theme::colors::kTextSecondary, theme::typography::kEmphasisStyle);
    centerTextOrigin(scoreText);
    scoreText.setOrigin(scoreText.getLocalBounds().left + scoreText.getLocalBounds().width,
                        scoreText.getOrigin().y);
    fitTextWidth(scoreText, theme::sizes::kLeaderboardScoreWidth,
                 theme::typography::minimum::kLeaderboardScore);
    scoreText.setPosition(columns.scoreRight, centerY);
    target.draw(scoreText);
}

void drawLeaderboardRow(sf::RenderTarget &target, const sf::Font &font, const sf::FloatRect bounds,
                        const std::size_t rank, const sf::String &name, const sf::String &score) {
    RoundedRectShape row({bounds.width, bounds.height}, theme::radius::kRow);
    row.setPosition(bounds.left, bounds.top);
    row.setFillColor(rank == 0U
                         ? theme::colors::kLeaderboardFirstRow
                         : (rank % 2U == 0U ? theme::colors::kRowEven : theme::colors::kRowOdd));
    target.draw(row);
    const auto columns = leaderboardColumns(bounds);
    const float centerY = bounds.top + bounds.height * 0.5f;
    const bool hasMedal = rank < theme::colors::kLeaderboardMedals.size();
    if (hasMedal) {
        const float radius = theme::sizes::kLeaderboardMedalDiameter / 2.f;
        const auto &colors = theme::colors::kLeaderboardMedals[rank];
        sf::CircleShape medal(radius, 2U * theme::sizes::kCornerPointCount);
        medal.setOrigin(radius, radius);
        medal.setPosition(columns.rankCenter, centerY);
        medal.setFillColor(colors.fill);
        medal.setOutlineColor(colors.border);
        medal.setOutlineThickness(theme::sizes::kLeaderboardMedalBorder);
        target.draw(medal);
    }
    sf::Text rankText(std::to_string(rank + 1U), font,
                      hasMedal ? theme::typography::kLeaderboardMedalNumberSize
                               : theme::typography::kLeaderboardRankSize);
    styleText(rankText, hasMedal ? theme::colors::kTextPrimary : theme::colors::kTextSecondary,
              hasMedal ? theme::typography::kEmphasisStyle : theme::typography::kBodyStyle);
    centerTextOrigin(rankText);
    rankText.setPosition(columns.rankCenter, centerY);
    target.draw(rankText);
    sf::Text scoreText(score, font, theme::typography::kLeaderboardScoreSize);
    styleText(scoreText, theme::colors::kTextPrimary, theme::typography::kLeaderboardScoreStyle);
    centerTextOrigin(scoreText);
    scoreText.setOrigin(scoreText.getLocalBounds().left + scoreText.getLocalBounds().width,
                        scoreText.getOrigin().y);
    fitTextWidth(scoreText, theme::sizes::kLeaderboardScoreWidth,
                 theme::typography::minimum::kLeaderboardScore);
    scoreText.setPosition(columns.scoreRight, centerY);
    target.draw(scoreText);
    sf::Text nameText(presentPlayerName(font, name).text, font,
                      theme::typography::kLeaderboardNameSize);
    styleText(nameText, theme::colors::kTextPrimary, theme::typography::kLeaderboardNameStyle);
    ellipsizeText(nameText, columns.nameWidth);
    centerTextOrigin(nameText);
    nameText.setOrigin(nameText.getLocalBounds().left, nameText.getOrigin().y);
    nameText.setPosition(columns.nameLeft, centerY);
    target.draw(nameText);
}

sf::RectangleShape createModalOverlay(const sf::Vector2f size) {
    sf::RectangleShape overlay(size);
    overlay.setFillColor(theme::colors::kModalDim);
    return overlay;
}

void drawModal(sf::RenderTarget &target, const sf::RectangleShape &overlay,
               const RoundedRectShape &panel) {
    target.draw(overlay);
    drawPanel(target, panel);
}

sf::Color getTileColor(const int value) {
    for (const auto &tile : theme::colors::kTilePalette) {
        if (tile.value == value) {
            return tile.fill;
        }
    }
    return theme::colors::kTileFallback;
}

sf::Color getTileTextColor(const int value) {
    for (const auto &tile : theme::colors::kTilePalette) {
        if (tile.value == value) {
            return tile.text;
        }
    }
    return theme::colors::kOnTile;
}

void drawEmptyCell(sf::RenderTarget &window, const sf::Vector2f &center, const float scale) {
    RoundedRectShape cell({static_cast<float>(kCellSize), static_cast<float>(kCellSize)},
                          kTileCornerRadius, kRoundedCornerPointCount);
    cell.setOrigin(cell.getSize() / 2.f);
    cell.setPosition(center);
    cell.setScale(scale, scale);
    cell.setFillColor(theme::colors::kEmptyTile);
    window.draw(cell);
}

TextFitResult drawTile(sf::RenderTarget &window, const sf::Font &font, const int value,
                       const sf::Vector2f &center, const float scale, const sf::Uint8 alpha) {
    if (value <= 0) {
        return {0.f, true};
    }

    RoundedRectShape cell({static_cast<float>(kCellSize), static_cast<float>(kCellSize)},
                          kTileCornerRadius, kRoundedCornerPointCount);
    cell.setOrigin(cell.getSize() / 2.f);
    cell.setScale(scale, scale);
    cell.setPosition(center);
    auto tileColor = getTileColor(value);
    tileColor.a = alpha;
    cell.setFillColor(tileColor);
    window.draw(cell);

    const unsigned int charSize = (value <= 64)     ? theme::typography::kTileShortSize
                                  : (value <= 512)  ? theme::typography::kTileMediumSize
                                  : (value <= 1024) ? theme::typography::kTileLongSize
                                                    : theme::typography::kTileExtraLongSize;
    sf::Text tileText(std::to_string(value), font, charSize);
    auto textColor = getTileTextColor(value);
    textColor.a = alpha;
    styleText(tileText, textColor, theme::typography::kTileStyle);
    const auto fit =
        fitTextWidth(tileText, static_cast<float>(kCellSize) - 2.f * theme::sizes::kTileTextInset,
                     theme::typography::minimum::kTile);
    centerTextOrigin(tileText);
    const float textScale = tileText.getScale().x * scale;
    tileText.setScale(textScale, textScale);
    tileText.setPosition(center);
    window.draw(tileText);
    const float effectiveSize = fit.effectiveSize * std::abs(scale);
    return {effectiveSize, fit.meetsMinimum && effectiveSize >= theme::typography::minimum::kTile};
}

std::optional<numtilo_core::Direction> mapDirection(const sf::Keyboard::Key key) {
    switch (key) {
    case sf::Keyboard::Up:
        return numtilo_core::Direction::Up;
    case sf::Keyboard::Down:
        return numtilo_core::Direction::Down;
    case sf::Keyboard::Left:
        return numtilo_core::Direction::Left;
    case sf::Keyboard::Right:
        return numtilo_core::Direction::Right;
    default:
        return std::nullopt;
    }
}

} // namespace app::ui
