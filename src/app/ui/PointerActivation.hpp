#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>

namespace app::ui {

// One control owns a primary-pointer gesture from an inside press to an inside
// release. Leaving its logical hit region cancels the gesture permanently.
class PointerActivation {
  public:
    template <typename HitTest>
    bool handleEvent(const sf::Event &event, const HitTest &containsPixel) {
        switch (event.type) {
        case sf::Event::MouseButtonPressed:
            if (event.mouseButton.button == sf::Mouse::Left) {
                pressed_ = containsPixel(sf::Vector2i{event.mouseButton.x, event.mouseButton.y});
            }
            break;
        case sf::Event::MouseButtonReleased:
            if (event.mouseButton.button == sf::Mouse::Left) {
                const bool activated = pressed_ && containsPixel(sf::Vector2i{event.mouseButton.x,
                                                                              event.mouseButton.y});
                reset();
                return activated;
            }
            break;
        case sf::Event::MouseMoved:
            if (pressed_ && !containsPixel(sf::Vector2i{event.mouseMove.x, event.mouseMove.y})) {
                reset();
            }
            break;
        case sf::Event::MouseLeft:
        case sf::Event::LostFocus:
            reset();
            break;
        default:
            break;
        }
        return false;
    }

    bool pressed() const {
        return pressed_;
    }

    void reset() {
        pressed_ = false;
    }

  private:
    bool pressed_{false};
};

} // namespace app::ui
