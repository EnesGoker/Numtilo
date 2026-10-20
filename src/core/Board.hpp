#pragma once

#include "core/Tile.hpp"

#include <array>
#include <vector>

namespace numtilo_core {

enum class Direction { Up, Down, Left, Right };

struct Cell {
    int row;
    int col;

    constexpr bool operator==(const Cell &other) const noexcept = default;

    constexpr bool operator<(const Cell &other) const noexcept {
        return row != other.row ? row < other.row : col < other.col;
    }
};

struct TileMovement {
    int value;
    Cell from;
    Cell to;
    bool partOfMerge{false};
};

struct TileMerge {
    Cell cell;
    int value;
};

struct BoardMoveResult {
    bool moved{false};
    int scoreDelta{0};
    std::vector<TileMovement> movements;
    std::vector<TileMerge> merges;
};

// Owns the grid and its movement rules; spawning and score accumulation belong to Game.
class Board {
  public:
    static constexpr int kGridSize = 4;
    using Grid = std::array<std::array<int, kGridSize>, kGridSize>;

    Board() = default;
    explicit Board(const Grid &grid);

    void clear() noexcept;
    void loadGrid(const Grid &grid) noexcept;
    const Grid &getGrid() const noexcept;

    BoardMoveResult applyMove(Direction direction);
    std::vector<Cell> emptyCells() const;
    bool placeTile(Cell cell, Tile tile) noexcept;
    bool hasAvailableMoves() const noexcept;

  private:
    Grid grid_{};
};

} // namespace numtilo_core
