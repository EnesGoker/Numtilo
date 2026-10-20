#include "app/GameSession.hpp"
#include "core/ScoreManager.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <random>
#include <set>
#include <string>
#include <type_traits>

namespace {

using app::GameSession;
using numtilo_core::Direction;
using numtilo_core::Game;
using numtilo_core::MoveResult;

constexpr std::array kDirections{Direction::Left, Direction::Up, Direction::Right, Direction::Down};

void requireSameGame(const Game &actual, const Game &expected) {
    REQUIRE(actual.getGrid() == expected.getGrid());
    REQUIRE(actual.getScore() == expected.getScore());
    REQUIRE(actual.hasWon() == expected.hasWon());
    REQUIRE(actual.isGameOver() == expected.isGameOver());
}

void requireSameResult(const MoveResult &actual, const MoveResult &expected) {
    REQUIRE(actual.moved == expected.moved);
    REQUIRE(actual.scoreDelta == expected.scoreDelta);
    REQUIRE(actual.spawnedTile.has_value() == expected.spawnedTile.has_value());
    if (expected.spawnedTile) {
        REQUIRE(actual.spawnedTile->row == expected.spawnedTile->row);
        REQUIRE(actual.spawnedTile->col == expected.spawnedTile->col);
        REQUIRE(actual.spawnedTile->value == expected.spawnedTile->value);
    }
    REQUIRE(actual.movements.size() == expected.movements.size());
    REQUIRE(actual.merges.size() == expected.merges.size());
}

void reachGameOver(GameSession &session) {
    const std::string gameId = session.gameId();
    constexpr std::size_t kMaximumTurns = 5000;
    std::size_t turn = 0;
    for (; turn < kMaximumTurns && !session.game().isGameOver(); ++turn) {
        session.applyMove(kDirections[turn % kDirections.size()]);
        REQUIRE(session.gameId() == gameId);
    }
    CAPTURE(turn);
    REQUIRE(session.game().isGameOver());
    REQUIRE(session.game().getScore() > 0);
}

void compareSeededTurns(GameSession &session, Game &reference) {
    std::mt19937 directionRng(0x137029U);
    const auto gameId = session.gameId();
    requireSameGame(session.game(), reference);
    for (int turn = 0; turn < 512; ++turn) {
        const auto direction = kDirections[directionRng() % kDirections.size()];
        requireSameResult(session.applyMove(direction), reference.applyMove(direction));
        requireSameGame(session.game(), reference);
        REQUIRE(session.gameId() == gameId);
    }
}

} // namespace

TEST_CASE("Every session and restart receives a distinct nonempty round identity", "[session]") {
    STATIC_REQUIRE(!std::is_copy_constructible_v<GameSession>);
    STATIC_REQUIRE(!std::is_copy_assignable_v<GameSession>);
    STATIC_REQUIRE(!std::is_move_constructible_v<GameSession>);
    STATIC_REQUIRE(!std::is_move_assignable_v<GameSession>);

    std::set<std::string> identities;
    GameSession defaultSession;
    REQUIRE_FALSE(defaultSession.gameId().empty());
    REQUIRE(identities.insert(defaultSession.gameId()).second);
    for (std::uint32_t seed = 0; seed < 32; ++seed) {
        GameSession session(seed);
        REQUIRE_FALSE(session.gameId().empty());
        REQUIRE(identities.insert(session.gameId()).second);
        for (int restart = 0; restart < 4; ++restart) {
            session.resetGame(seed);
            REQUIRE_FALSE(session.gameId().empty());
            REQUIRE(identities.insert(session.gameId()).second);
        }
    }

    defaultSession.resetGame();
    REQUIRE_FALSE(defaultSession.gameId().empty());
    REQUIRE(identities.insert(defaultSession.gameId()).second);
}

TEST_CASE("Session identity and bookkeeping do not alter seeded gameplay", "[session]") {
    constexpr std::array<std::uint32_t, 3> kSeeds{0U, 12345U, 0xFFFFFFFFU};
    for (const auto seed : kSeeds) {
        CAPTURE(seed);
        GameSession session(seed);
        Game reference(seed);
        session.setPlayerName("Oyuncu A");
        session.markFinalScoreRecorded();
        compareSeededTurns(session, reference);

        const auto previousId = session.gameId();
        session.resetGame(seed);
        reference.reset(seed);
        REQUIRE(session.gameId() != previousId);
        REQUIRE(session.playerName() == "Oyuncu A");
        REQUIRE(session.game().getScore() == 0);
        REQUIRE_FALSE(session.shouldRecordFinalScore());
        compareSeededTurns(session, reference);
    }
}

TEST_CASE("Changing player metadata preserves round identity and core state", "[session]") {
    GameSession session(128U);
    const auto gameId = session.gameId();
    const auto initialGrid = session.game().getGrid();
    session.setPlayerName("Enes");
    REQUIRE(session.playerName() == "Enes");
    REQUIRE(session.gameId() == gameId);
    REQUIRE(session.game().getGrid() == initialGrid);
    session.setPlayerName("");
    REQUIRE(session.playerName() == "Oyuncu");
    REQUIRE(session.gameId() == gameId);
    REQUIRE(session.game().getGrid() == initialGrid);
}

TEST_CASE("Completion is acknowledged once and premature acknowledgement is ignored", "[session]") {
    GameSession session(9U);
    session.setPlayerName("Inci");
    const auto gameId = session.gameId();
    REQUIRE_FALSE(session.shouldRecordFinalScore());
    session.markFinalScoreRecorded();
    REQUIRE_FALSE(session.shouldRecordFinalScore());

    reachGameOver(session);
    REQUIRE(session.gameId() == gameId);
    REQUIRE(session.shouldRecordFinalScore());
    for (const auto direction : kDirections) {
        const auto result = session.applyMove(direction);
        REQUIRE_FALSE(result.moved);
        REQUIRE_FALSE(result.spawnedTile.has_value());
        REQUIRE(session.gameId() == gameId);
        REQUIRE(session.shouldRecordFinalScore());
    }

    session.markFinalScoreRecorded();
    REQUIRE_FALSE(session.shouldRecordFinalScore());
    session.markFinalScoreRecorded();
    REQUIRE_FALSE(session.shouldRecordFinalScore());
    REQUIRE(session.gameId() == gameId);
    for (const auto direction : kDirections) {
        session.applyMove(direction);
        REQUIRE_FALSE(session.shouldRecordFinalScore());
        REQUIRE(session.gameId() == gameId);
    }

    session.resetGame(9U);
    REQUIRE(session.gameId() != gameId);
    REQUIRE(session.playerName() == "Inci");
    REQUIRE(session.game().getScore() == 0);
    REQUIRE_FALSE(session.game().isGameOver());
    REQUIRE_FALSE(session.shouldRecordFinalScore());
    reachGameOver(session);
    REQUIRE(session.shouldRecordFinalScore());
}

TEST_CASE("An unseeded restart clears completed-round bookkeeping", "[session]") {
    GameSession session(9U);
    reachGameOver(session);
    session.markFinalScoreRecorded();
    REQUIRE_FALSE(session.shouldRecordFinalScore());
    const auto completedId = session.gameId();
    session.setPlayerName("Enes");

    session.resetGame();
    REQUIRE(session.gameId() != completedId);
    REQUIRE(session.playerName() == "Enes");
    REQUIRE(session.game().getScore() == 0);
    REQUIRE_FALSE(session.game().hasWon());
    REQUIRE_FALSE(session.game().isGameOver());
    REQUIRE_FALSE(session.shouldRecordFinalScore());
}

TEST_CASE("Restart keeps profile best and completed history independent of the active round",
          "[session][score][memory]") {
    numtilo_core::ScoreManager profile(std::filesystem::path{});
    GameSession session(9U);
    const auto abandonedId = session.gameId();
    for (std::size_t turn = 0; turn < 64 && session.game().getScore() == 0; ++turn) {
        session.applyMove(kDirections[turn % kDirections.size()]);
    }
    REQUIRE(session.game().getScore() > 0);
    REQUIRE_FALSE(session.game().isGameOver());
    REQUIRE(profile.updateBestScore(session.game().getScore()));
    const auto ongoingBest = profile.bestScore();

    session.resetGame(9U);
    REQUIRE(session.gameId() != abandonedId);
    REQUIRE(session.game().getScore() == 0);
    REQUIRE(profile.bestScore() == ongoingBest);
    REQUIRE(profile.completedGames().empty());

    reachGameOver(session);
    REQUIRE(session.shouldRecordFinalScore());
    REQUIRE(profile.recordCompletedGame(session.gameId(), session.game().getScore(),
                                        session.playerName(), "2026-10-06T09:00:00Z"));
    session.markFinalScoreRecorded();
    const auto completedId = session.gameId();
    const auto completedBest = profile.bestScore();
    REQUIRE_FALSE(session.shouldRecordFinalScore());
    REQUIRE_FALSE(profile.recordCompletedGame(completedId, session.game().getScore()));
    REQUIRE(profile.completedGames().size() == 1);

    session.resetGame(9U);
    REQUIRE(session.gameId() != completedId);
    REQUIRE(profile.bestScore() == completedBest);
    REQUIRE(profile.completedGames().size() == 1);
    REQUIRE(profile.completedGames().front().gameId == completedId);
    REQUIRE(profile.isDirty());
}
