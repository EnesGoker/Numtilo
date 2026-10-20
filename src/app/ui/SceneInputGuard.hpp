#pragma once

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>

#include <set>

namespace app::ui {

// Held keys and primary-pointer gestures belong to their originating scene.
class SceneInputGuard {
  public:
    bool accepts(const sf::Event &event) {
        switch (event.type) {
        case sf::Event::KeyPressed:
            if (isTrackedKey(event.key.code)) {
                if (focused_) {
                    pressed_.insert(event.key.code);
                } else {
                    blocked_.insert(event.key.code);
                }
            }
            return focused_ && !blocked_.contains(event.key.code);
        case sf::Event::KeyReleased:
            pressed_.erase(event.key.code);
            return blocked_.erase(event.key.code) == 0U && focused_;
        case sf::Event::TextEntered:
            return focused_ && blocked_.empty();
        case sf::Event::LostFocus:
            onSceneChanged();
            pressed_.clear();
            focused_ = false;
            return true;
        case sf::Event::GainedFocus:
            focused_ = true;
            // Releases may occur while another application owns window focus.
            std::erase_if(blocked_,
                          [](const auto key) { return !sf::Keyboard::isKeyPressed(key); });
            return true;
        case sf::Event::MouseButtonPressed:
            if (event.mouseButton.button == sf::Mouse::Left) {
                // Unlike keys, primary pointer presses do not repeat while held.
                // An observed fresh press therefore starts a new scene-owned gesture.
                primaryPointerOwned_ = focused_;
            }
            return focused_;
        case sf::Event::MouseButtonReleased:
            if (event.mouseButton.button == sf::Mouse::Left) {
                const bool accepted = focused_ && primaryPointerOwned_;
                primaryPointerOwned_ = false;
                return accepted;
            }
            return focused_;
        case sf::Event::MouseLeft:
            primaryPointerOwned_ = false;
            return true;
        default:
            return true;
        }
    }

    void onSceneChanged() {
        blocked_.insert(pressed_.begin(), pressed_.end());
        primaryPointerOwned_ = false;
    }

  private:
    static bool isTrackedKey(const sf::Keyboard::Key key) {
        if (key < 0 || key >= sf::Keyboard::KeyCount) {
            return false;
        }
        switch (key) {
        case sf::Keyboard::LShift:
        case sf::Keyboard::RShift:
        case sf::Keyboard::LControl:
        case sf::Keyboard::RControl:
        case sf::Keyboard::LAlt:
        case sf::Keyboard::RAlt:
        case sf::Keyboard::LSystem:
        case sf::Keyboard::RSystem:
            return false;
        default:
            return true;
        }
    }

    std::set<sf::Keyboard::Key> pressed_;
    std::set<sf::Keyboard::Key> blocked_;
    bool focused_{true};
    bool primaryPointerOwned_{false};
};

} // namespace app::ui
