#include "app/ui/GameplayOutcome.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

using app::ui::GameplayOutcome;
using app::ui::SceneId;
using numtilo_core::Direction;
using numtilo_core::Game;

constexpr Game::Grid kWinningMerge{{{1024, 1024, 0, 0}, {}, {}, {}}};
constexpr Game::Grid kTerminalWinningMerge{
    {{1024, 1024, 4, 8}, {2, 8, 16, 32}, {4, 16, 32, 64}, {8, 32, 64, 128}}};
constexpr Game::Grid kDeadGrid{{{2, 4, 2, 4}, {4, 2, 4, 2}, {2, 4, 2, 4}, {4, 2, 4, 2}}};

} // namespace

TEST_CASE("Win presentation waits for the complete move animation", "[ui][win-presentation]") {
    Game game(0);
    game.loadState(kWinningMerge);
    GameplayOutcome outcome;
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-1", game, false) == SceneId::Playing);
    REQUIRE(game.applyMove(Direction::Left).moved);
    REQUIRE(game.hasWon());
    for (int frame = 0; frame < 20; ++frame) {
        REQUIRE(outcome.nextScene(SceneId::Playing, "round-1", game, true) == SceneId::Playing);
    }
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-1", game, false) == SceneId::Win);
}

TEST_CASE("A presented win stays dismissed throughout the same round", "[ui][win-presentation]") {
    Game game(0);
    game.loadState(kWinningMerge);
    game.applyMove(Direction::Left);
    GameplayOutcome outcome;
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-1", game, false) == SceneId::Win);
    for (int frame = 0; frame < 20; ++frame) {
        REQUIRE(outcome.nextScene(SceneId::Win, "round-1", game, false) == SceneId::Win);
    }
    // Returning to Playing models the later Continue command without adding it in 7.1.
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-1", game, false) == SceneId::Playing);
    REQUIRE(game.applyMove(Direction::Right).moved);
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-1", game, false) == SceneId::Playing);
    game.loadState({{{2048, 2048, 0, 0}, {}, {}, {}}}, game.getScore());
    REQUIRE(game.applyMove(Direction::Left).moved);
    REQUIRE(game.getGrid()[0][0] == 4096);
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-1", game, false) == SceneId::Playing);
}

TEST_CASE("A new round resets win presentation without a focus reset", "[ui][win-presentation]") {
    Game game(0);
    game.loadState(kWinningMerge);
    game.applyMove(Direction::Left);
    GameplayOutcome outcome;
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-1", game, false) == SceneId::Win);
    game.reset(0);
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-2", game, false) == SceneId::Playing);
    game.loadState(kWinningMerge);
    game.applyMove(Direction::Left);
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-2", game, true) == SceneId::Playing);
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-2", game, false) == SceneId::Win);
}

TEST_CASE("A terminal winning move presents Win before Game Over", "[ui][win-presentation]") {
    Game game(0);
    game.loadState(kTerminalWinningMerge);
    REQUIRE(game.applyMove(Direction::Left).moved);
    REQUIRE(game.hasWon());
    REQUIRE(game.isGameOver());
    GameplayOutcome outcome;
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-1", game, true) == SceneId::Playing);
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-1", game, false) == SceneId::Win);
    REQUIRE(outcome.nextScene(SceneId::Win, "round-1", game, false) == SceneId::Win);
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-1", game, false) == SceneId::GameOver);
}

TEST_CASE("Only Playing can consume a round outcome", "[ui][win-presentation]") {
    Game game(0);
    game.loadState(kWinningMerge);
    game.applyMove(Direction::Left);
    GameplayOutcome outcome;
    for (const auto scene : {SceneId::Splash, SceneId::HighScores, SceneId::GameOver}) {
        REQUIRE(outcome.nextScene(scene, "round-1", game, false) == scene);
    }
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-1", game, false) == SceneId::Win);
    game.loadState(kDeadGrid);
    REQUIRE_FALSE(game.hasWon());
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-2", game, true) == SceneId::Playing);
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-2", game, false) == SceneId::GameOver);
}

TEST_CASE("Presentation preserves the board score flags and gameplay RNG",
          "[ui][win-presentation]") {
    Game actual(73);
    actual.loadState(kWinningMerge, 100);
    actual.applyMove(Direction::Left);
    Game expected = actual;
    GameplayOutcome outcome;
    for (int frame = 0; frame < 20; ++frame) {
        outcome.nextScene(SceneId::Playing, "round-1", actual, true);
    }
    REQUIRE(outcome.nextScene(SceneId::Playing, "round-1", actual, false) == SceneId::Win);
    REQUIRE(actual.getGrid() == expected.getGrid());
    REQUIRE(actual.getScore() == expected.getScore());
    REQUIRE(actual.hasWon() == expected.hasWon());
    REQUIRE(actual.isGameOver() == expected.isGameOver());
    for (const auto direction :
         {Direction::Right, Direction::Down, Direction::Left, Direction::Up}) {
        const auto observed = actual.applyMove(direction);
        const auto control = expected.applyMove(direction);
        REQUIRE(observed.moved == control.moved);
        REQUIRE(observed.scoreDelta == control.scoreDelta);
        REQUIRE(actual.getGrid() == expected.getGrid());
        REQUIRE(actual.getScore() == expected.getScore());
    }
}
