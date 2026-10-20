#pragma once

namespace numtilo_core {

// Tile values and merge rules are independent of how a tile is drawn.
struct Tile {
    int value{0};

    constexpr bool isEmpty() const noexcept {
        return value == 0;
    }

    constexpr bool canMergeWith(const Tile &other) const noexcept {
        return !isEmpty() && value == other.value;
    }

    constexpr Tile merged() const noexcept {
        return Tile{value * 2};
    }
};

} // namespace numtilo_core
