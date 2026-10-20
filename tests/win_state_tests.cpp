#include "core/Game.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace {

using numtilo_core::Cell;
using numtilo_core::Direction;
using numtilo_core::Game;
using numtilo_core::MoveResult;

using Line = std::array<int, Game::kGridSize>;

constexpr std::array kDirections{Direction::Left, Direction::Right, Direction::Up, Direction::Down};
constexpr int kInitialScore = 37;
constexpr Game::Grid kDeadGrid{{
    {2, 4, 8, 16},
    {32, 64, 128, 256},
    {512, 1024, 2, 4},
    {8, 16, 32, 64},
}};
constexpr Game::Grid kWonDeadGrid{{
    {2, 4, 8, 16},
    {32, 64, 128, 256},
    {512, 2048, 2, 4},
    {8, 16, 32, 64},
}};
constexpr Game::Grid kPlayableGrid{{
    {0, 0, 0, 0},
    {0, 2, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
}};
constexpr Game::Grid kWonPlayableGrid{{
    {0, 0, 0, 0},
    {0, 2048, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
}};

static_assert(Game::kWinningTileValue == 2048);
static_assert(noexcept(std::declval<const Game &>().hasWon()));
static_assert(noexcept(std::declval<const Game &>().isGameOver()));

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

bool movesTowardFirstCell(const Direction direction) {
    return direction == Direction::Left || direction == Direction::Up;
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

// The helper lays out physical coordinates; expected merge outcomes remain literal test data.
Cell lineCell(const Direction direction, const int lineIndex, const int cellIndex) {
    if (direction == Direction::Left || direction == Direction::Right) {
        return {lineIndex, cellIndex};
    }
    return {cellIndex, lineIndex};
}

Game::Grid lineGrid(const Direction direction, const int lineIndex, const Line &line) {
    Game::Grid grid{};
    for (int cellIndex = 0; cellIndex < Game::kGridSize; ++cellIndex) {
        const auto cell = lineCell(direction, lineIndex, cellIndex);
        grid[cell.row][cell.col] = line[cellIndex];
    }
    return grid;
}

void requireState(const Game &game, const int score, const bool won, const bool gameOver) {
    REQUIRE(game.getScore() == score);
    REQUIRE(game.getState().score == score);
    REQUIRE(game.getState().won == won);
    REQUIRE(game.hasWon() == won);
    REQUIRE(game.getState().gameOver == gameOver);
    REQUIRE(game.isGameOver() == gameOver);
}

void requireGridAfterSpawn(const Game &game, const Game::Grid &postMove, const MoveResult &result,
                           const bool spawnOnMove) {
    REQUIRE(result.spawnedTile.has_value() == spawnOnMove);
    auto expected = postMove;
    if (result.spawnedTile) {
        const auto &spawn = *result.spawnedTile;
        CAPTURE(spawn.row, spawn.col, spawn.value);
        REQUIRE(spawn.row >= 0);
        REQUIRE(spawn.row < Game::kGridSize);
        REQUIRE(spawn.col >= 0);
        REQUIRE(spawn.col < Game::kGridSize);
        REQUIRE((spawn.value == 2 || spawn.value == 4));
        REQUIRE(expected[spawn.row][spawn.col] == 0);
        expected[spawn.row][spawn.col] = spawn.value;
    }
    REQUIRE(game.getGrid() == expected);
}

void requireSameTurn(const Game &actual, const MoveResult &actualTurn, const Game &expected,
                     const MoveResult &expectedTurn) {
    REQUIRE(actualTurn.moved == expectedTurn.moved);
    REQUIRE(actualTurn.scoreDelta == expectedTurn.scoreDelta);
    REQUIRE(actualTurn.spawnedTile.has_value() == expectedTurn.spawnedTile.has_value());
    if (actualTurn.spawnedTile) {
        REQUIRE(actualTurn.spawnedTile->row == expectedTurn.spawnedTile->row);
        REQUIRE(actualTurn.spawnedTile->col == expectedTurn.spawnedTile->col);
        REQUIRE(actualTurn.spawnedTile->value == expectedTurn.spawnedTile->value);
    }
    REQUIRE(actual.getGrid() == expected.getGrid());
    requireState(actual, expected.getScore(), expected.hasWon(), expected.isGameOver());
}

void requireNewRound(const Game &game) {
    int occupied = 0;
    for (const auto &row : game.getGrid()) {
        for (const int value : row) {
            REQUIRE((value == 0 || value == 2 || value == 4));
            occupied += value != 0;
        }
    }
    REQUIRE(occupied == 2);
    requireState(game, 0, false, false);
}

} // namespace

TEST_CASE("Every direction and line position recognizes a winning merge with or without spawning",
          "[win][win-matrix][state]") {
    constexpr Line input{1024, 0, 1024, 0};
    constexpr Line forward{2048, 0, 0, 0};
    constexpr Line backward{0, 0, 0, 2048};
    for (const auto direction : kDirections) {
        for (int lineIndex = 0; lineIndex < Game::kGridSize; ++lineIndex) {
            for (const bool spawnOnMove : {false, true}) {
                DYNAMIC_SECTION(directionName(direction)
                                << ", line " << lineIndex << ", spawn " << spawnOnMove) {
                    Game game(0x2048C0DEU);
                    game.loadState(lineGrid(direction, lineIndex, input), kInitialScore);
                    requireState(game, kInitialScore, false, false);

                    const auto result = game.applyMove(direction, spawnOnMove);
                    REQUIRE(result.moved);
                    REQUIRE(result.scoreDelta == 2048);
                    REQUIRE(result.merges.size() == 1);
                    REQUIRE(result.merges.front().value == 2048);
                    const auto expected = lineGrid(
                        direction, lineIndex, movesTowardFirstCell(direction) ? forward : backward);
                    requireGridAfterSpawn(game, expected, result, spawnOnMove);
                    requireState(game, kInitialScore + 2048, true, false);
                }
            }
        }
    }
}

TEST_CASE("Winning does not block later moves or a merge beyond the winning value",
          "[win][win-latch][state]") {
    constexpr Line input{1024, 1024, 1024, 1024};
    constexpr Line firstForward{2048, 2048, 0, 0};
    constexpr Line firstBackward{0, 0, 2048, 2048};
    constexpr Line secondForward{4096, 0, 0, 0};
    constexpr Line secondBackward{0, 0, 0, 4096};
    for (const auto direction : kDirections) {
        for (int lineIndex = 0; lineIndex < Game::kGridSize; ++lineIndex) {
            DYNAMIC_SECTION(directionName(direction) << ", line " << lineIndex) {
                Game game(73);
                game.loadState(lineGrid(direction, lineIndex, input), kInitialScore);
                requireState(game, kInitialScore, false, false);

                const auto first = game.applyMove(direction, false);
                REQUIRE(first.moved);
                REQUIRE(first.scoreDelta == 4096);
                REQUIRE_FALSE(first.spawnedTile.has_value());
                REQUIRE(game.getGrid() ==
                        lineGrid(direction, lineIndex,
                                 movesTowardFirstCell(direction) ? firstForward : firstBackward));
                requireState(game, kInitialScore + 4096, true, false);

                const auto second = game.applyMove(direction, false);
                REQUIRE(second.moved);
                REQUIRE(second.scoreDelta == 4096);
                REQUIRE_FALSE(second.spawnedTile.has_value());
                REQUIRE(game.getGrid() ==
                        lineGrid(direction, lineIndex,
                                 movesTowardFirstCell(direction) ? secondForward : secondBackward));
                requireState(game, kInitialScore + 4096 + 4096, true, false);

                const auto slide = game.applyMove(opposite(direction), false);
                REQUIRE(slide.moved);
                REQUIRE(slide.scoreDelta == 0);
                REQUIRE_FALSE(slide.spawnedTile.has_value());
                REQUIRE(game.getGrid() ==
                        lineGrid(direction, lineIndex,
                                 movesTowardFirstCell(direction) ? secondBackward : secondForward));
                requireState(game, kInitialScore + 4096 + 4096, true, false);
            }
        }
    }
}

TEST_CASE("No-op moves preserve won and score for both playable and terminal rounds",
          "[win][win-latch][invalid-move][state]") {
    for (const auto direction : kDirections) {
        for (const bool gameOver : {false, true}) {
            DYNAMIC_SECTION(directionName(direction) << ", game over " << gameOver) {
                Game game(91);
                const auto initial =
                    gameOver ? kWonDeadGrid
                             : lineGrid(direction, 0,
                                        movesTowardFirstCell(direction) ? Line{2048, 0, 0, 0}
                                                                        : Line{0, 0, 0, 2048});
                game.loadState(initial, kInitialScore);
                requireState(game, kInitialScore, true, gameOver);

                const auto result = game.applyMove(direction);
                REQUIRE_FALSE(result.moved);
                REQUIRE(result.scoreDelta == 0);
                REQUIRE_FALSE(result.spawnedTile.has_value());
                REQUIRE(result.movements.empty());
                REQUIRE(result.merges.empty());
                REQUIRE(game.getGrid() == initial);
                requireState(game, kInitialScore, true, gameOver);
            }
        }
    }
}

TEST_CASE("Won and game over represent all four independent state combinations",
          "[win][state][game-over][load-state]") {
    struct Fixture {
        std::string_view name;
        Game::Grid grid;
        bool won;
        bool gameOver;
    };
    constexpr std::array fixtures{
        Fixture{"unwon and playable", kPlayableGrid, false, false},
        Fixture{"unwon and terminal", kDeadGrid, false, true},
        Fixture{"won and playable", kWonPlayableGrid, true, false},
        Fixture{"won and terminal", kWonDeadGrid, true, true},
    };
    for (const auto &fixture : fixtures) {
        DYNAMIC_SECTION(fixture.name) {
            Game game(123);
            game.loadState(fixture.grid, kInitialScore);
            REQUIRE(game.getGrid() == fixture.grid);
            requireState(game, kInitialScore, fixture.won, fixture.gameOver);
        }
    }
}

TEST_CASE("A winning merge and the last spawn can end a round in the same turn",
          "[win][state][game-over][spawn]") {
    struct Fixture {
        Direction direction;
        Game::Grid initial;
        Game::Grid postMove;
        Cell emptyCell;
    };
    constexpr std::array fixtures{
        Fixture{Direction::Left,
                {{{1024, 1024, 4, 8}, {2, 8, 16, 32}, {4, 16, 32, 64}, {8, 32, 64, 128}}},
                {{{2048, 4, 8, 0}, {2, 8, 16, 32}, {4, 16, 32, 64}, {8, 32, 64, 128}}},
                {0, 3}},
        Fixture{Direction::Right,
                {{{8, 4, 1024, 1024}, {32, 16, 8, 2}, {64, 32, 16, 4}, {128, 64, 32, 8}}},
                {{{0, 8, 4, 2048}, {32, 16, 8, 2}, {64, 32, 16, 4}, {128, 64, 32, 8}}},
                {0, 0}},
        Fixture{Direction::Up,
                {{{1024, 2, 4, 8}, {1024, 8, 16, 32}, {4, 16, 32, 64}, {8, 32, 64, 128}}},
                {{{2048, 2, 4, 8}, {4, 8, 16, 32}, {8, 16, 32, 64}, {0, 32, 64, 128}}},
                {3, 0}},
        Fixture{Direction::Down,
                {{{8, 32, 64, 128}, {4, 16, 32, 64}, {1024, 8, 16, 32}, {1024, 2, 4, 8}}},
                {{{0, 32, 64, 128}, {8, 16, 32, 64}, {4, 8, 16, 32}, {2048, 2, 4, 8}}},
                {0, 0}},
    };
    for (const auto &fixture : fixtures) {
        for (const bool spawnOnMove : {false, true}) {
            DYNAMIC_SECTION(directionName(fixture.direction) << ", spawn " << spawnOnMove) {
                Game game(2048);
                game.loadState(fixture.initial, kInitialScore);
                requireState(game, kInitialScore, false, false);

                const auto result = game.applyMove(fixture.direction, spawnOnMove);
                REQUIRE(result.moved);
                REQUIRE(result.scoreDelta == 2048);
                requireGridAfterSpawn(game, fixture.postMove, result, spawnOnMove);
                if (result.spawnedTile) {
                    REQUIRE(result.spawnedTile->row == fixture.emptyCell.row);
                    REQUIRE(result.spawnedTile->col == fixture.emptyCell.col);
                }
                requireState(game, kInitialScore + 2048, true, spawnOnMove);
            }
        }
    }
}

TEST_CASE("Loading recognizes the winning threshold and larger values in every cell",
          "[win][win-threshold][load-state][state]") {
    constexpr std::array values{2, 4, 512, 1024, 2048, 4096, 8192};
    for (const int value : values) {
        for (int row = 0; row < Game::kGridSize; ++row) {
            for (int col = 0; col < Game::kGridSize; ++col) {
                DYNAMIC_SECTION("value " << value << ", cell " << row << "," << col) {
                    Game::Grid grid{};
                    grid[row][col] = value;
                    Game game(29);
                    game.loadState(grid, kInitialScore);
                    REQUIRE(game.getGrid() == grid);
                    requireState(game, kInitialScore, value >= 2048, false);
                }
            }
        }
    }
}

TEST_CASE("Loading a replacement round clears the previous round's win and terminal flags",
          "[win][load-state][state]") {
    Game game(61);
    game.loadState(kWonDeadGrid, 9000);
    requireState(game, 9000, true, true);

    game.loadState(kPlayableGrid, kInitialScore);
    REQUIRE(game.getGrid() == kPlayableGrid);
    requireState(game, kInitialScore, false, false);

    game.loadState(kDeadGrid, 0);
    REQUIRE(game.getGrid() == kDeadGrid);
    requireState(game, 0, false, true);

    game.loadState(kWonPlayableGrid, 2048);
    requireState(game, 2048, true, false);

    game.loadState(Game::Grid{});
    REQUIRE(game.getGrid() == Game::Grid{});
    requireState(game, 0, false, false);
}

TEST_CASE("Replacing round flags by loading does not reseed or consume the random sequence",
          "[win][load-state][rng][state]") {
    for (std::uint32_t sample = 0; sample < 32; ++sample) {
        const auto seed = 0x2048F17EU + sample;
        CAPTURE(sample, seed);
        Game actual(seed);
        Game expected(seed);
        actual.loadState(kPlayableGrid, kInitialScore);
        expected.loadState(kPlayableGrid, kInitialScore);
        const auto actualFirst = actual.applyMove(Direction::Left);
        const auto expectedFirst = expected.applyMove(Direction::Left);
        REQUIRE(actualFirst.moved);
        requireSameTurn(actual, actualFirst, expected, expectedFirst);

        // Only actual is reloaded after both RNGs have advanced by an ordinary spawn.
        actual.loadState(kWonDeadGrid, 9000);
        requireState(actual, 9000, true, true);
        actual.loadState(expected.getGrid(), expected.getScore());
        requireState(actual, expected.getScore(), false, false);
        for (int turn = 0; turn < 8; ++turn) {
            const auto direction = kDirections[static_cast<std::size_t>(turn) % kDirections.size()];
            CAPTURE(turn, directionName(direction));
            const auto actualTurn = actual.applyMove(direction);
            const auto expectedTurn = expected.applyMove(direction);
            requireSameTurn(actual, actualTurn, expected, expectedTurn);
        }
    }
}

TEST_CASE("Seeded reset clears won and game over and restores a fresh game's random sequence",
          "[win][reset][rng][state]") {
    for (std::uint32_t sample = 0; sample < 32; ++sample) {
        const auto seed = 0x204851A7U + sample;
        CAPTURE(sample, seed);
        Game actual(seed ^ 0xFFFFFFFFU);
        actual.loadState(kWonDeadGrid, 9000);
        requireState(actual, 9000, true, true);

        actual.reset(seed);
        Game expected(seed);
        requireNewRound(actual);
        REQUIRE(actual.getGrid() == expected.getGrid());
        for (int turn = 0; turn < 8; ++turn) {
            const auto direction = kDirections[static_cast<std::size_t>(turn) % kDirections.size()];
            CAPTURE(turn, directionName(direction));
            const auto actualTurn = actual.applyMove(direction);
            const auto expectedTurn = expected.applyMove(direction);
            requireSameTurn(actual, actualTurn, expected, expectedTurn);
        }
    }
}

TEST_CASE("Unseeded reset clears both flags after a won terminal round", "[win][reset][state]") {
    Game game(83);
    game.loadState(kWonDeadGrid, 9000);
    requireState(game, 9000, true, true);
    game.reset();
    requireNewRound(game);
}
