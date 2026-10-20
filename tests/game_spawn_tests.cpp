#include "core/Board.hpp"
#include "core/Game.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

using numtilo_core::Board;
using numtilo_core::Cell;
using numtilo_core::Direction;
using numtilo_core::Game;
using numtilo_core::MoveResult;
using numtilo_core::SpawnedTile;
using numtilo_core::TileMerge;
using numtilo_core::TileMovement;

constexpr std::array kDirections{Direction::Left, Direction::Right, Direction::Up, Direction::Down};
constexpr int kInitialScore = 37;
constexpr Game::Grid kDeadGrid{{
    {2, 4, 8, 16},
    {32, 64, 128, 256},
    {512, 1024, 2, 4},
    {8, 16, 32, 64},
}};

std::string_view directionName(const Direction direction) {
    switch (direction) {
    case Direction::Left:
        return "left";
    case Direction::Right:
        return "right";
    case Direction::Up:
        return "up";
    case Direction::Down:
        return "down";
    }
    return "unknown";
}

Direction opposite(const Direction direction) {
    switch (direction) {
    case Direction::Left:
        return Direction::Right;
    case Direction::Right:
        return Direction::Left;
    case Direction::Up:
        return Direction::Down;
    case Direction::Down:
        return Direction::Up;
    }
    return Direction::Left;
}

Cell targetCorner(const Direction direction) {
    switch (direction) {
    case Direction::Left:
    case Direction::Up:
        return {0, 0};
    case Direction::Right:
        return {0, Board::kGridSize - 1};
    case Direction::Down:
        return {Board::kGridSize - 1, 0};
    }
    return {0, 0};
}

int tileCount(const Game::Grid &grid) {
    int count = 0;
    for (const auto &row : grid) {
        for (const int value : row) {
            count += value != 0;
        }
    }
    return count;
}

int tileSum(const Game::Grid &grid) {
    int sum = 0;
    for (const auto &row : grid) {
        for (const int value : row) {
            sum += value;
        }
    }
    return sum;
}

using MovementRecord = std::tuple<int, int, int, int, int, bool>;
using MergeRecord = std::tuple<int, int, int>;

std::vector<MovementRecord> movementRecords(const std::vector<TileMovement> &movements) {
    std::vector<MovementRecord> records;
    for (const auto &movement : movements) {
        records.emplace_back(movement.value, movement.from.row, movement.from.col, movement.to.row,
                             movement.to.col, movement.partOfMerge);
    }
    std::sort(records.begin(), records.end());
    return records;
}

std::vector<MergeRecord> mergeRecords(const std::vector<TileMerge> &merges) {
    std::vector<MergeRecord> records;
    for (const auto &merge : merges) {
        records.emplace_back(merge.cell.row, merge.cell.col, merge.value);
    }
    std::sort(records.begin(), records.end());
    return records;
}

void requireSameSpawn(const MoveResult &actual, const MoveResult &expected) {
    REQUIRE(actual.spawnedTile.has_value());
    REQUIRE(expected.spawnedTile.has_value());
    REQUIRE(actual.spawnedTile->row == expected.spawnedTile->row);
    REQUIRE(actual.spawnedTile->col == expected.spawnedTile->col);
    REQUIRE(actual.spawnedTile->value == expected.spawnedTile->value);
}

void requireOneSafeSpawn(const Game::Grid &postMove, const Game::Grid &after,
                         const SpawnedTile &spawn) {
    CAPTURE(spawn.row, spawn.col, spawn.value);
    REQUIRE((spawn.value == 2 || spawn.value == 4));
    REQUIRE(spawn.row >= 0);
    REQUIRE(spawn.row < Game::kGridSize);
    REQUIRE(spawn.col >= 0);
    REQUIRE(spawn.col < Game::kGridSize);
    // Occupancy is checked after sliding/merging, before spawning.
    REQUIRE(postMove[spawn.row][spawn.col] == 0);

    auto expected = postMove;
    expected[spawn.row][spawn.col] = spawn.value;
    REQUIRE(after == expected);
    REQUIRE(tileCount(after) == tileCount(postMove) + 1);
    REQUIRE(tileSum(after) == tileSum(postMove) + spawn.value);
}

MoveResult requireSpawnTurn(Game &game, const Game::Grid &initial, const Direction direction) {
    Board movedBoard(initial);
    const auto boardMove = movedBoard.applyMove(direction);
    game.loadState(initial, kInitialScore);
    const auto result = game.applyMove(direction);
    REQUIRE(result.moved == boardMove.moved);
    REQUIRE(result.scoreDelta == boardMove.scoreDelta);
    REQUIRE(game.getScore() == kInitialScore + boardMove.scoreDelta);
    REQUIRE(game.getState().score == game.getScore());
    REQUIRE(movementRecords(result.movements) == movementRecords(boardMove.movements));
    REQUIRE(mergeRecords(result.merges) == mergeRecords(boardMove.merges));

    if (boardMove.moved) {
        REQUIRE(result.spawnedTile.has_value());
        requireOneSafeSpawn(movedBoard.getGrid(), game.getGrid(), *result.spawnedTile);
    } else {
        REQUIRE_FALSE(result.spawnedTile.has_value());
        REQUIRE(game.getGrid() == initial);
    }
    const bool available = Board(game.getGrid()).hasAvailableMoves();
    REQUIRE(game.isGameOver() == !available);
    REQUIRE(game.getState().gameOver == !available);
    REQUIRE(game.hasWon() == game.getState().won);
    return result;
}

void requireNewRound(const Game &game) {
    REQUIRE(tileCount(game.getGrid()) == 2);
    for (const auto &row : game.getGrid()) {
        for (const int value : row) {
            REQUIRE((value == 0 || value == 2 || value == 4));
        }
    }
    REQUIRE(Board(game.getGrid()).emptyCells().size() == 14);
    REQUIRE(game.getScore() == 0);
    REQUIRE(game.getState().score == 0);
    REQUIRE_FALSE(game.getState().won);
    REQUIRE_FALSE(game.hasWon());
    REQUIRE_FALSE(game.getState().gameOver);
    REQUIRE_FALSE(game.isGameOver());
}

Game::Grid distinctFullGrid() {
    Game::Grid grid{};
    for (int row = 0; row < Game::kGridSize; ++row) {
        for (int col = 0; col < Game::kGridSize; ++col) {
            grid[row][col] = 1 << (row * Game::kGridSize + col + 1);
        }
    }
    return grid;
}

} // namespace

TEST_CASE("New rounds contain exactly two allowed normal tiles and reset all round state",
          "[spawn][spawn-invariant][reset]") {
    bool observedTwo = false;
    bool observedFour = false;
    std::mt19937 seeds(0x204852E7U);
    for (int sample = 0; sample < 256; ++sample) {
        const auto seed = static_cast<std::uint32_t>(seeds());
        CAPTURE(sample, seed);
        Game fresh(seed);
        requireNewRound(fresh);
        for (const auto &row : fresh.getGrid()) {
            for (const int value : row) {
                observedTwo = observedTwo || value == 2;
                observedFour = observedFour || value == 4;
            }
        }

        Game resetGame(seed ^ 0xFFFFFFFFU);
        resetGame.loadState(kDeadGrid, 9999);
        REQUIRE(resetGame.isGameOver());
        resetGame.reset(seed);
        requireNewRound(resetGame);
        REQUIRE(resetGame.getGrid() == fresh.getGrid());
        for (const auto direction : kDirections) {
            const auto actual = resetGame.applyMove(direction);
            const auto expected = fresh.applyMove(direction);
            REQUIRE(actual.moved == expected.moved);
            REQUIRE(actual.spawnedTile.has_value() == expected.spawnedTile.has_value());
            if (actual.spawnedTile) {
                requireSameSpawn(actual, expected);
            }
            REQUIRE(resetGame.getGrid() == fresh.getGrid());
            REQUIRE(resetGame.getScore() == fresh.getScore());
            REQUIRE(resetGame.getState().gameOver == fresh.getState().gameOver);
            REQUIRE(resetGame.getState().won == fresh.getState().won);
        }
    }
    // Reachability checks, not probabilistic frequency thresholds.
    REQUIRE(observedTwo);
    REQUIRE(observedFour);
}

TEST_CASE("Spawn changes exactly one post-move empty cell across fixture boards",
          "[spawn][spawn-invariant]") {
    constexpr std::array<Game::Grid, 6> fixtures{{
        {},
        {{{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 2}}},
        {{{2, 2, 0, 8}, {0, 0, 0, 0}, {0, 4, 0, 0}, {16, 0, 16, 0}}},
        {{{2, 2, 2, 2}, {2, 2, 2, 2}, {2, 2, 2, 2}, {2, 2, 2, 2}}},
        {{{2, 2, 8, 16}, {32, 64, 128, 256}, {512, 1024, 2048, 4096}, {8192, 16384, 32768, 65536}}},
        kDeadGrid,
    }};
    for (std::uint32_t seed = 0; seed < 32; ++seed) {
        for (std::size_t fixture = 0; fixture < fixtures.size(); ++fixture) {
            for (const auto direction : kDirections) {
                INFO("seed=" << seed << " fixture=" << fixture
                             << " direction=" << directionName(direction));
                Game game(seed);
                requireSpawnTurn(game, fixtures[fixture], direction);
            }
        }
    }
}

TEST_CASE("The only remaining empty cell is used without overwriting any other cell",
          "[spawn][spawn-invariant]") {
    for (std::uint32_t seed = 0; seed < 16; ++seed) {
        for (int row = 0; row < Game::kGridSize; ++row) {
            for (int col = 0; col < Game::kGridSize; ++col) {
                auto initial = distinctFullGrid();
                initial[row][col] = 0;
                for (const auto direction : kDirections) {
                    INFO("seed=" << seed << " empty=" << row << ',' << col
                                 << " direction=" << directionName(direction));
                    Game game(seed);
                    const auto result = requireSpawnTurn(game, initial, direction);
                    if (result.moved) {
                        REQUIRE(tileCount(game.getGrid()) == 16);
                        REQUIRE(result.scoreDelta == 0);
                    } else {
                        REQUIRE(tileCount(game.getGrid()) == 15);
                    }
                }
            }
        }
    }
}

TEST_CASE("Generated valid boards obey the normal spawn and non-overwrite invariants",
          "[spawn][spawn-invariant][property]") {
    constexpr std::uint32_t corpusSeed = 0x20485A04U;
    constexpr std::array values{0, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048};
    std::mt19937 generator(corpusSeed);
    for (int sample = 0; sample < 200; ++sample) {
        Game::Grid initial{};
        for (auto &row : initial) {
            for (int &value : row) {
                value = values[generator() % values.size()];
            }
        }
        const auto gameSeed = static_cast<std::uint32_t>(generator());
        for (const auto direction : kDirections) {
            INFO("corpus=" << corpusSeed << " sample=" << sample << " gameSeed=" << gameSeed
                           << " direction=" << directionName(direction));
            Game game(gameSeed);
            requireSpawnTurn(game, initial, direction);
        }
    }
}

TEST_CASE("No-op moves neither spawn tiles nor consume the future spawn sequence",
          "[spawn][spawn-invariant][rng]") {
    for (std::uint32_t seed = 0; seed < 64; ++seed) {
        for (const auto direction : kDirections) {
            INFO("seed=" << seed << " no-op=" << directionName(direction));
            Game::Grid initial{};
            const auto cell = targetCorner(direction);
            initial[cell.row][cell.col] = 2;
            Game actual(seed);
            Game control(seed);
            actual.loadState(initial, kInitialScore);
            control.loadState(initial, kInitialScore);
            for (int repeat = 0; repeat < 5; ++repeat) {
                const auto noOp = actual.applyMove(direction);
                REQUIRE_FALSE(noOp.moved);
                REQUIRE_FALSE(noOp.spawnedTile.has_value());
                REQUIRE(noOp.scoreDelta == 0);
                REQUIRE(noOp.movements.empty());
                REQUIRE(noOp.merges.empty());
                REQUIRE(actual.getGrid() == initial);
                REQUIRE(actual.getScore() == kInitialScore);
                REQUIRE_FALSE(actual.getState().gameOver);
            }
            const auto nextActual = actual.applyMove(opposite(direction));
            const auto nextControl = control.applyMove(opposite(direction));
            requireSameSpawn(nextActual, nextControl);
            REQUIRE(actual.getGrid() == control.getGrid());
            REQUIRE(actual.getScore() == control.getScore());
        }
    }
}

TEST_CASE("Disabling spawn on a valid move preserves the future spawn sequence",
          "[spawn][spawn-invariant][rng]") {
    for (std::uint32_t seed = 0; seed < 64; ++seed) {
        for (const auto direction : kDirections) {
            INFO("seed=" << seed << " disabled move=" << directionName(direction));
            Game::Grid initial{};
            const auto cell = targetCorner(opposite(direction));
            initial[cell.row][cell.col] = 2;
            Game actual(seed);
            Game control(seed);
            actual.loadState(initial, kInitialScore);
            const auto disabled = actual.applyMove(direction, false);
            REQUIRE(disabled.moved);
            REQUIRE_FALSE(disabled.spawnedTile.has_value());
            REQUIRE(tileCount(actual.getGrid()) == 1);
            REQUIRE(tileSum(actual.getGrid()) == 2);
            REQUIRE(actual.getScore() == kInitialScore);
            control.loadState(actual.getGrid(), actual.getScore());

            const auto nextActual = actual.applyMove(opposite(direction));
            const auto nextControl = control.applyMove(opposite(direction));
            requireSameSpawn(nextActual, nextControl);
            REQUIRE(actual.getGrid() == control.getGrid());
            REQUIRE(actual.getScore() == control.getScore());
        }
    }
}

TEST_CASE("Game over is computed after the final empty cell is spawned into",
          "[spawn][gameover][spawn-invariant]") {
    auto initial = kDeadGrid;
    initial[0][0] = 0;
    for (std::uint32_t seed = 0; seed < 64; ++seed) {
        CAPTURE(seed);
        Game game(seed);
        game.loadState(initial, kInitialScore);
        REQUIRE_FALSE(game.isGameOver());
        const auto result = requireSpawnTurn(game, initial, Direction::Left);
        REQUIRE(result.spawnedTile.has_value());
        REQUIRE(result.spawnedTile->row == 0);
        REQUIRE(result.spawnedTile->col == 3);
        REQUIRE(tileCount(game.getGrid()) == 16);
        REQUIRE(game.isGameOver());
        REQUIRE(game.getState().gameOver);
        REQUIRE(game.getScore() == kInitialScore);
    }
}

TEST_CASE("A full board remains playable if either possible spawn creates a legal merge",
          "[spawn][gameover][spawn-invariant]") {
    constexpr Game::Grid initial{{
        {16, 8, 0, 4},
        {32, 64, 128, 2},
        {256, 512, 1024, 2048},
        {4096, 8192, 16384, 32768},
    }};
    bool observedTwo = false;
    bool observedFour = false;
    for (std::uint32_t seed = 0; seed < 64; ++seed) {
        CAPTURE(seed);
        Game game(seed);
        const auto result = requireSpawnTurn(game, initial, Direction::Left);
        REQUIRE(result.spawnedTile.has_value());
        REQUIRE(result.spawnedTile->row == 0);
        REQUIRE(result.spawnedTile->col == 3);
        REQUIRE(tileCount(game.getGrid()) == 16);
        REQUIRE_FALSE(game.isGameOver());
        REQUIRE_FALSE(game.getState().gameOver);
        observedTwo = observedTwo || result.spawnedTile->value == 2;
        observedFour = observedFour || result.spawnedTile->value == 4;
    }
    REQUIRE(observedTwo);
    REQUIRE(observedFour);
}
