#include "core/Board.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace numtilo_core {

namespace {

Cell cellAtLineIndex(const int line, const int index, const Direction direction) {
    switch (direction) {
    case Direction::Left:
        return Cell{line, index};
    case Direction::Right:
        return Cell{line, Board::kGridSize - 1 - index};
    case Direction::Up:
        return Cell{index, line};
    case Direction::Down:
        return Cell{Board::kGridSize - 1 - index, line};
    }

    return Cell{line, index};
}

struct LineToken {
    Tile tile;
    Cell from;
};

} // namespace

Board::Board(const Grid &grid) : grid_(grid) {
}

void Board::clear() noexcept {
    grid_ = {};
}

void Board::loadGrid(const Grid &grid) noexcept {
    grid_ = grid;
}

const Board::Grid &Board::getGrid() const noexcept {
    return grid_;
}

BoardMoveResult Board::applyMove(const Direction direction) {
    switch (direction) {
    case Direction::Up:
    case Direction::Down:
    case Direction::Left:
    case Direction::Right:
        break;
    default:
        return {};
    }

    BoardMoveResult result;

    for (int line = 0; line < kGridSize; ++line) {
        std::array<int, kGridSize> before{};
        std::array<int, kGridSize> after{};
        std::vector<LineToken> tokens;
        tokens.reserve(kGridSize);

        for (int index = 0; index < kGridSize; ++index) {
            const Cell cell = cellAtLineIndex(line, index, direction);
            const Tile tile{grid_[cell.row][cell.col]};
            before[index] = tile.value;
            if (!tile.isEmpty()) {
                tokens.push_back(LineToken{tile, cell});
            }
        }

        int targetIndex = 0;
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            const Cell to = cellAtLineIndex(line, targetIndex, direction);
            const bool mergesWithNext =
                i + 1U < tokens.size() && tokens[i].tile.canMergeWith(tokens[i + 1U].tile);

            if (mergesWithNext) {
                const Tile merged = tokens[i].tile.merged();
                after[targetIndex] = merged.value;
                result.scoreDelta += merged.value;
                // Both original tiles participate, even if the first is already at the target.
                result.movements.push_back(
                    TileMovement{tokens[i].tile.value, tokens[i].from, to, true});
                result.movements.push_back(
                    TileMovement{tokens[i + 1U].tile.value, tokens[i + 1U].from, to, true});
                result.merges.push_back(TileMerge{to, merged.value});
                ++i;
            } else {
                after[targetIndex] = tokens[i].tile.value;
                if (tokens[i].from != to) {
                    result.movements.push_back(
                        TileMovement{tokens[i].tile.value, tokens[i].from, to, false});
                }
            }

            ++targetIndex;
        }

        result.moved = result.moved || before != after;

        for (int index = 0; index < kGridSize; ++index) {
            const Cell cell = cellAtLineIndex(line, index, direction);
            grid_[cell.row][cell.col] = after[index];
        }
    }

    return result;
}

std::vector<Cell> Board::emptyCells() const {
    std::vector<Cell> cells;
    cells.reserve(kGridSize * kGridSize);

    // Game relies on this stable row-major order for seeded spawning.
    for (int row = 0; row < kGridSize; ++row) {
        for (int col = 0; col < kGridSize; ++col) {
            if (Tile{grid_[row][col]}.isEmpty()) {
                cells.push_back(Cell{row, col});
            }
        }
    }

    return cells;
}

bool Board::placeTile(const Cell cell, const Tile tile) noexcept {
    if (cell.row < 0 || cell.row >= kGridSize || cell.col < 0 || cell.col >= kGridSize ||
        tile.isEmpty()) {
        return false;
    }

    if (!Tile{grid_[cell.row][cell.col]}.isEmpty()) {
        return false;
    }

    grid_[cell.row][cell.col] = tile.value;
    return true;
}

bool Board::hasAvailableMoves() const noexcept {
    for (int row = 0; row < kGridSize; ++row) {
        for (int col = 0; col < kGridSize; ++col) {
            const Tile tile{grid_[row][col]};
            if (tile.isEmpty()) {
                return true;
            }

            if (col + 1 < kGridSize && tile.canMergeWith(Tile{grid_[row][col + 1]})) {
                return true;
            }

            if (row + 1 < kGridSize && tile.canMergeWith(Tile{grid_[row + 1][col]})) {
                return true;
            }
        }
    }

    return false;
}

} // namespace numtilo_core
