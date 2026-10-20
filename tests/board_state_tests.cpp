#include "core/Board.hpp"
#include "core/Game.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <limits>
#include <vector>

namespace {

using numtilo_core::Board;
using numtilo_core::Cell;
using numtilo_core::Direction;
using numtilo_core::Game;
using numtilo_core::Tile;

constexpr std::array kDirections{Direction::Left, Direction::Right, Direction::Up, Direction::Down};

// All cells differ, so replacing one value can isolate exactly one matching pair.
// The largest value is 65536; even its doubled value is well within int bounds.
Board::Grid distinctFullGrid() {
    Board::Grid grid{};
    for (int row = 0; row < Board::kGridSize; ++row) {
        for (int col = 0; col < Board::kGridSize; ++col) {
            grid[row][col] = 1 << (row * Board::kGridSize + col + 1);
        }
    }
    return grid;
}

void requireEmptyCells(const Board::Grid &grid, const std::vector<Cell> &expected) {
    const Board board(grid);
    const auto firstQuery = board.emptyCells();
    REQUIRE(firstQuery == expected);
    REQUIRE(board.emptyCells() == firstQuery);
    REQUIRE(board.getGrid() == grid);
}

void requireAvailability(const Board::Grid &grid, const bool expected) {
    const Board board(grid);
    REQUIRE(board.hasAvailableMoves() == expected);

    // Cross-check the state query against moves on independent board copies. The
    // empty board has available cells but cannot move any tile yet.
    if (board.emptyCells().size() != Board::kGridSize * Board::kGridSize) {
        bool anyDirectionMoves = false;
        for (const Direction direction : kDirections) {
            Board copy(grid);
            anyDirectionMoves = copy.applyMove(direction).moved || anyDirectionMoves;
        }
        REQUIRE(anyDirectionMoves == expected);
    }

    Game game(0x2048U);
    game.loadState(grid, 73);
    REQUIRE(game.getGrid() == grid);
    REQUIRE(game.getScore() == 73);
    REQUIRE(game.getState().score == 73);
    REQUIRE(game.getState().gameOver == !expected);
    REQUIRE(game.isGameOver() == !expected);
    REQUIRE(game.hasWon() == game.getState().won);

    if (!expected) {
        for (const Direction direction : kDirections) {
            CAPTURE(direction);
            const auto result = game.applyMove(direction);
            REQUIRE_FALSE(result.moved);
            REQUIRE(result.scoreDelta == 0);
            REQUIRE_FALSE(result.spawnedTile.has_value());
            REQUIRE(result.movements.empty());
            REQUIRE(result.merges.empty());
            REQUIRE(game.getGrid() == grid);
            REQUIRE(game.getScore() == 73);
            REQUIRE(game.getState().score == 73);
            REQUIRE(game.getState().gameOver);
            REQUIRE(game.isGameOver());
        }
    }
}

} // namespace

TEST_CASE("empty cells are exact and stable in row-major order", "[board][empty-cells]") {
    SECTION("empty board reports every cell") {
        const std::vector<Cell> expected{{0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 0}, {1, 1},
                                         {1, 2}, {1, 3}, {2, 0}, {2, 1}, {2, 2}, {2, 3},
                                         {3, 0}, {3, 1}, {3, 2}, {3, 3}};
        requireEmptyCells({}, expected);
    }

    SECTION("full board has no empty cells") {
        requireEmptyCells(distinctFullGrid(), {});
    }

    SECTION("mixed holes preserve order and omit occupied cells") {
        auto grid = distinctFullGrid();
        const std::vector<Cell> expected{{0, 1}, {0, 3}, {1, 0}, {2, 2}, {3, 0}, {3, 3}};
        for (const Cell cell : expected) {
            grid[cell.row][cell.col] = 0;
        }
        requireEmptyCells(grid, expected);
    }

    SECTION("a single empty cell is found at every position") {
        for (int row = 0; row < Board::kGridSize; ++row) {
            for (int col = 0; col < Board::kGridSize; ++col) {
                CAPTURE(row, col);
                auto grid = distinctFullGrid();
                grid[row][col] = 0;
                requireEmptyCells(grid, {Cell{row, col}});
            }
        }
    }
}

TEST_CASE("tile placement changes only the selected vacant cell", "[board][place-tile]") {
    constexpr std::array values{2, 4, 2048, 65536};
    for (int row = 0; row < Board::kGridSize; ++row) {
        for (int col = 0; col < Board::kGridSize; ++col) {
            for (const int value : values) {
                CAPTURE(row, col, value);
                auto before = distinctFullGrid();
                before[row][col] = 0;
                Board board(before);
                REQUIRE(board.placeTile(Cell{row, col}, Tile{value}));

                auto expected = before;
                expected[row][col] = value;
                REQUIRE(board.getGrid() == expected);
                REQUIRE(board.emptyCells().empty());
            }
        }
    }
}

TEST_CASE("tile placement never overwrites an occupied cell", "[board][place-tile][invariant]") {
    constexpr std::array values{2, 4, 2048, 65536};
    for (int row = 0; row < Board::kGridSize; ++row) {
        for (int col = 0; col < Board::kGridSize; ++col) {
            for (const int existingValue : values) {
                auto before = distinctFullGrid();
                before[row][col] = existingValue;
                Board board(before);
                for (const int incomingValue : values) {
                    CAPTURE(row, col, existingValue, incomingValue);
                    REQUIRE_FALSE(board.placeTile(Cell{row, col}, Tile{incomingValue}));
                    REQUIRE(board.getGrid() == before);
                    REQUIRE(board.emptyCells().empty());
                }
            }
        }
    }
}

TEST_CASE("rejected placement leaves the entire board unchanged", "[board][place-tile][boundary]") {
    SECTION("an empty tile is rejected at every valid cell") {
        for (int row = 0; row < Board::kGridSize; ++row) {
            for (int col = 0; col < Board::kGridSize; ++col) {
                CAPTURE(row, col);
                auto grid = distinctFullGrid();
                grid[row][col] = 0;
                Board board(grid);
                REQUIRE_FALSE(board.placeTile(Cell{row, col}, Tile{}));
                REQUIRE(board.getGrid() == grid);
                REQUIRE((board.emptyCells() == std::vector<Cell>{Cell{row, col}}));
            }
        }
    }

    SECTION("out-of-bounds rows and columns are rejected before indexing") {
        constexpr std::array invalidIndices{-1, Board::kGridSize, std::numeric_limits<int>::min(),
                                            std::numeric_limits<int>::max()};
        constexpr std::array values{2, 4, 2048};
        const auto grid = distinctFullGrid();
        Board board(grid);
        for (const int invalid : invalidIndices) {
            for (int valid = 0; valid < Board::kGridSize; ++valid) {
                for (const int value : values) {
                    CAPTURE(invalid, valid, value);
                    REQUIRE_FALSE(board.placeTile(Cell{invalid, valid}, Tile{value}));
                    REQUIRE(board.getGrid() == grid);
                    REQUIRE_FALSE(board.placeTile(Cell{valid, invalid}, Tile{value}));
                    REQUIRE(board.getGrid() == grid);
                }
            }
            REQUIRE_FALSE(board.placeTile(Cell{invalid, invalid}, Tile{2}));
            REQUIRE(board.getGrid() == grid);
        }
    }
}

TEST_CASE("every orthogonal pair keeps a full board playable", "[board][gameover][boundary]") {
    for (int row = 0; row < Board::kGridSize; ++row) {
        for (int col = 0; col + 1 < Board::kGridSize; ++col) {
            DYNAMIC_SECTION("horizontal pair at " << row << "," << col) {
                auto grid = distinctFullGrid();
                grid[row][col + 1] = grid[row][col];
                requireAvailability(grid, true);
            }
        }
    }

    for (int row = 0; row + 1 < Board::kGridSize; ++row) {
        for (int col = 0; col < Board::kGridSize; ++col) {
            DYNAMIC_SECTION("vertical pair at " << row << "," << col) {
                auto grid = distinctFullGrid();
                grid[row + 1][col] = grid[row][col];
                requireAvailability(grid, true);
            }
        }
    }
}

TEST_CASE("diagonal and wrapped matches do not prevent game over", "[board][gameover][boundary]") {
    SECTION("distinct full board is terminal") {
        requireAvailability(distinctFullGrid(), false);
    }

    SECTION("diagonal matches are not adjacent") {
        for (int row = 0; row + 1 < Board::kGridSize; ++row) {
            for (int col = 0; col + 1 < Board::kGridSize; ++col) {
                DYNAMIC_SECTION("descending diagonal at " << row << "," << col) {
                    auto grid = distinctFullGrid();
                    grid[row + 1][col + 1] = grid[row][col];
                    requireAvailability(grid, false);
                }
                DYNAMIC_SECTION("ascending diagonal at " << row << "," << col) {
                    auto grid = distinctFullGrid();
                    grid[row + 1][col] = grid[row][col + 1];
                    requireAvailability(grid, false);
                }
            }
        }
    }

    SECTION("consecutive flattened rows do not share a horizontal edge") {
        for (int row = 0; row + 1 < Board::kGridSize; ++row) {
            CAPTURE(row);
            auto grid = distinctFullGrid();
            grid[row + 1][0] = grid[row][Board::kGridSize - 1];
            requireAvailability(grid, false);
        }
    }

    SECTION("opposite horizontal and vertical edges do not wrap") {
        for (int line = 0; line < Board::kGridSize; ++line) {
            DYNAMIC_SECTION("horizontal edges of row " << line) {
                auto grid = distinctFullGrid();
                grid[line][Board::kGridSize - 1] = grid[line][0];
                requireAvailability(grid, false);
            }
            DYNAMIC_SECTION("vertical edges of column " << line) {
                auto grid = distinctFullGrid();
                grid[Board::kGridSize - 1][line] = grid[0][line];
                requireAvailability(grid, false);
            }
        }
    }

    SECTION("checkerboard has many equal diagonals but no valid move") {
        const Board::Grid grid{{{{2, 4, 2, 4}}, {{4, 2, 4, 2}}, {{2, 4, 2, 4}}, {{4, 2, 4, 2}}}};
        requireAvailability(grid, false);
    }
}

TEST_CASE("a vacant cell keeps the board playable at every position", "[board][gameover]") {
    for (int row = 0; row < Board::kGridSize; ++row) {
        for (int col = 0; col < Board::kGridSize; ++col) {
            CAPTURE(row, col);
            auto grid = distinctFullGrid();
            grid[row][col] = 0;
            requireAvailability(grid, true);
        }
    }
    requireAvailability({}, true);
}

TEST_CASE("loading a playable board clears stale terminal state", "[game][gameover][load-state]") {
    const auto deadGrid = distinctFullGrid();
    Game game(0xB04DU);
    for (int row = 0; row < Board::kGridSize; ++row) {
        for (int col = 0; col < Board::kGridSize; ++col) {
            CAPTURE(row, col);
            game.loadState(deadGrid, 137);
            REQUIRE(game.getState().won);
            REQUIRE(game.hasWon());
            REQUIRE(game.getState().gameOver);
            REQUIRE(game.isGameOver());
            REQUIRE(game.getScore() == 137);
            REQUIRE(game.getState().score == 137);

            auto playableGrid = deadGrid;
            playableGrid[row][col] = 0;
            game.loadState(playableGrid, 29);
            REQUIRE(game.getGrid() == playableGrid);
            REQUIRE(game.getState().won);
            REQUIRE(game.hasWon());
            REQUIRE_FALSE(game.getState().gameOver);
            REQUIRE_FALSE(game.isGameOver());
            REQUIRE(game.getScore() == 29);
            REQUIRE(game.getState().score == 29);
        }
    }

    game.loadState(deadGrid, 99);
    REQUIRE(game.isGameOver());
    auto mergeableFullGrid = deadGrid;
    mergeableFullGrid[3][3] = mergeableFullGrid[3][2];
    game.loadState(mergeableFullGrid, 11);
    REQUIRE(game.getGrid() == mergeableFullGrid);
    REQUIRE(game.getState().won);
    REQUIRE(game.hasWon());
    REQUIRE_FALSE(game.getState().gameOver);
    REQUIRE_FALSE(game.isGameOver());
    REQUIRE(game.getScore() == 11);
    REQUIRE(game.getState().score == 11);

    game.loadState(deadGrid, 0);
    REQUIRE(game.getState().gameOver);
    REQUIRE(game.isGameOver());
    game.loadState({});
    REQUIRE(game.getGrid() == Board::Grid{});
    REQUIRE_FALSE(game.getState().won);
    REQUIRE_FALSE(game.hasWon());
    REQUIRE_FALSE(game.getState().gameOver);
    REQUIRE_FALSE(game.isGameOver());
    REQUIRE(game.getScore() == 0);
    REQUIRE(game.getState().score == 0);
}
