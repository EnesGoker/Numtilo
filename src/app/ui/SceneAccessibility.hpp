#pragma once

#include "app/ui/Accessibility.hpp"
#include <SFML/Graphics.hpp>
#include <utility>

namespace app::ui {

inline std::string accessibleUtf8(const sf::String &text) {
    const auto bytes = text.toUtf8();
    return {bytes.begin(), bytes.end()};
}

inline AccessibilityNode accessibleNode(std::string id, AccessibilityRole role, std::string name,
                                        const sf::FloatRect &bounds, std::string parent = {}) {
    AccessibilityNode node;
    node.id = std::move(id);
    node.parentId = std::move(parent);
    node.role = role;
    node.name = std::move(name);
    node.frame = {bounds.left, bounds.top, bounds.width, bounds.height};
    return node;
}

inline AccessibilityNode accessibleButton(std::string id, std::string name,
                                          const sf::FloatRect &bounds, bool focused) {
    auto node = accessibleNode(std::move(id), AccessibilityRole::Button, std::move(name), bounds);
    node.focusable = true;
    node.focused = focused;
    node.actions = {AccessibilityActionKind::Press, AccessibilityActionKind::Focus};
    return node;
}

} // namespace app::ui
