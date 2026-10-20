#include "app/GameSession.hpp"
#include "app/ScoreCoordinator.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

using app::GameSession;
using app::ScoreCoordinator;
using numtilo_core::Direction;
using numtilo_core::Game;
using numtilo_core::ScoreEntry;
using numtilo_core::ScoreManager;

constexpr std::array kDirections{Direction::Left, Direction::Up, Direction::Right, Direction::Down};

// Seed 0 reaches its first 2048 at score 21196 through legal moves with normal spawning.
// The sequence was found using copies of the public Game API; the test performs no search,
// state injection, private access, or dependency on platform-specific distribution behavior.
constexpr std::string_view kWinningMoves =
    "DDLLDLLLDLDDLDUDLRLRLDLDUDRDLDLRDLDLUDRDLLDLRDLRRLDUDLDRDLLLLDDUDULULUDLLDLULLDL"
    "LDDUUDDLLDDRLUDLUDLDDLRDDDLDRRRDDLLDRDLRDRLLDLRDDDLDDUDLLRUDLDLLDDLDDDRDLLDDRDLU"
    "DLRDLRDLUUDLRDDUDLLDLRDDDLRLDLLDRLLDLDRDRDRRRDDRLDRDLRDDDDDUDLRDLDLLRDRDDLLDLDRD"
    "DLLDLLDLLDLLRDDDUDUDDUDDDLDRLDRLDLDDDLLLDULDLRLDDLRDDLLRDDLDLLDLDLUDLLDRLUDDRDLD"
    "RLRDDRRDRDLDLDDUDLDRLRRDDLDLDLRDDDDLDDLRDLRDRUDRLRLRDLUUDRLULDRRLDLRLDRDLDRDLDLR"
    "LRDLLRDLUDLLLRDLRDDUDLDLDRDRLDLRDLRDDDLRDRLDRDLDDRLDDLDLLDRDLDRDDRDLRDRRRLRRDLDD"
    "DDDDLUUDUDUDRLRLUDDRDLLDDLLDUDLULUUDDDLDDLLDULULUDRLDUDDLLUDRDDLRDLLRDLDDLRDDLLU"
    "RURDLDRLDLDRUDLLUUDLRLRDRDDDRDLDDLRDLRDLRDDLLDDLRRLLDRLDRDRLRDUDLLDDRDLDDRDDDLDL"
    "RDDLLRUDDLRRDRRDDLUDLLLRLDDLLRRDDDLDDDLLLDDLDLRDDLDDLDRDDRDLDLLLDLDRLRDDLLDLDLLD"
    "DRLLRLLLRDRLDLLLLDLRLRRLLRRLRLDDLDLRLDRDLDRDLRDLLLRRDLDDDRDRDDLRRDRDLRLRDRLLRDDD"
    "LDLDRDLDLDLDLDDLRDLLLRDRDLRDDLLLLLDDRRDRLDLDRLLDRLDLRDLLLLLRDLDRDDRDLLRDDDLRDLDL"
    "LDDLLDLRLDLRDDLDDLDLRLDRDDDRDRRDLLDDRDRDLDLLDRRLRLDRRDRDLLDLDLLDRDLDDRRDLLLRRRDD"
    "LLLLLDRDDLDLDLLDLDDRDLLLLDLDLDDLLLRLRLRRDLLDRDRRLLLDLLRDDLLDRURLDLDLL";

class AcceptanceDirectory {
  public:
    AcceptanceDirectory() {
        static std::atomic<unsigned long long> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 16; ++attempt) {
            path_ = std::filesystem::temp_directory_path() /
                    ("numtilo_score_acceptance_" + std::to_string(timestamp) + "_" +
                     std::to_string(sequence.fetch_add(1)));
            if (std::filesystem::create_directory(path_)) {
                return;
            }
        }
        throw std::runtime_error("could not create score acceptance directory");
    }

    ~AcceptanceDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    AcceptanceDirectory(const AcceptanceDirectory &) = delete;
    AcceptanceDirectory &operator=(const AcceptanceDirectory &) = delete;

    std::filesystem::path scoreFile() const {
        return path_ / "scores.json";
    }

  private:
    std::filesystem::path path_;
};

std::string profileBytes(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.is_open());
    const std::string bytes{std::istreambuf_iterator<char>(input),
                            std::istreambuf_iterator<char>()};
    REQUIRE_FALSE(input.bad());
    return bytes;
}

Direction fixtureDirection(const char value) {
    switch (value) {
    case 'L':
        return Direction::Left;
    case 'U':
        return Direction::Up;
    case 'R':
        return Direction::Right;
    case 'D':
        return Direction::Down;
    default:
        FAIL("invalid winning fixture direction");
        return Direction::Left;
    }
}

void finishRound(GameSession &session) {
    for (std::size_t turn = 0; turn < 5000 && !session.game().isGameOver(); ++turn) {
        session.applyMove(kDirections[turn % kDirections.size()]);
    }
    REQUIRE(session.game().isGameOver());
    REQUIRE(session.shouldRecordFinalScore());
}

void requireSameEntry(const ScoreEntry &actual, const ScoreEntry &expected) {
    REQUIRE(actual.gameId == expected.gameId);
    REQUIRE(actual.score == expected.score);
    REQUIRE(actual.playerName == expected.playerName);
    REQUIRE(actual.playedAtUtc == expected.playedAtUtc);
}

} // namespace

TEST_CASE("a real won round continues under one identity and is stored only at logical game over",
          "[score][acceptance][win]") {
    AcceptanceDirectory directory;
    ScoreManager manager(directory.scoreFile());
    REQUIRE(manager.load());
    GameSession session(0U);
    session.setPlayerName("İnci 🎮");
    ScoreCoordinator coordinator(manager);
    const auto roundId = session.gameId();

    for (const auto move : kWinningMoves) {
        REQUIRE(session.applyMove(fixtureDirection(move)).moved);
    }
    REQUIRE(session.game().hasWon());
    REQUIRE_FALSE(session.game().isGameOver());
    REQUIRE(session.game().getScore() == 21196);
    REQUIRE(session.gameId() == roundId);
    REQUIRE_FALSE(session.shouldRecordFinalScore());

    const auto winningCheckpoint = coordinator.checkpoint(session);
    REQUIRE(winningCheckpoint.bestImproved);
    REQUIRE(winningCheckpoint.firstBestOfRound);
    REQUIRE_FALSE(winningCheckpoint.roundCompleted);
    REQUIRE(winningCheckpoint.saveSucceeded);
    REQUIRE(manager.completedGames().empty());
    REQUIRE_FALSE(manager.isDirty());

    ScoreManager wonProfile(directory.scoreFile());
    REQUIRE(wonProfile.load());
    REQUIRE(wonProfile.bestScore() == 21196);
    REQUIRE(wonProfile.completedGames().empty());

    // Continue changes only the visible overlay. Normal moves keep the same session;
    // every changed move uses the same checkpoint contract as the actual app loop.
    bool continuedWhilePlayable = false;
    int completionNotifications = 0;
    for (std::size_t turn = 0; turn < 5000 && !session.game().isGameOver(); ++turn) {
        const auto move = session.applyMove(kDirections[turn % kDirections.size()]);
        if (!move.moved) {
            continue;
        }
        const auto checkpoint = coordinator.checkpoint(session);
        REQUIRE(session.gameId() == roundId);
        REQUIRE(session.game().hasWon());
        REQUIRE_FALSE(checkpoint.firstBestOfRound);
        REQUIRE_FALSE(checkpoint.failureToReport.has_value());
        completionNotifications += checkpoint.roundCompleted ? 1 : 0;
        if (!session.game().isGameOver()) {
            continuedWhilePlayable = true;
            REQUIRE_FALSE(checkpoint.roundCompleted);
            REQUIRE(manager.completedGames().empty());
        }
    }
    REQUIRE(continuedWhilePlayable);
    REQUIRE(session.game().isGameOver());
    REQUIRE(session.game().hasWon());
    REQUIRE(completionNotifications == 1);
    REQUIRE_FALSE(session.shouldRecordFinalScore());
    REQUIRE(manager.completedGames().size() == 1);
    const auto completed = manager.completedGames().front();
    REQUIRE(completed.gameId == roundId);
    REQUIRE(completed.score == session.game().getScore());
    REQUIRE(completed.playerName == "İnci 🎮");
    REQUIRE_FALSE(completed.playedAtUtc.empty());
    REQUIRE_FALSE(manager.isDirty());

    const auto savedBytes = profileBytes(directory.scoreFile());
    REQUIRE_FALSE(coordinator.checkpoint(session).saveAttempted);
    ScoreManager reopened(directory.scoreFile());
    REQUIRE(reopened.load());
    REQUIRE(reopened.bestScore() == session.game().getScore());
    REQUIRE(reopened.completedGames().size() == 1);
    requireSameEntry(reopened.completedGames().front(), completed);
    REQUIRE_FALSE(reopened.recordCompletedGame(roundId, std::numeric_limits<int>::max(), "Replay"));
    REQUIRE_FALSE(reopened.isDirty());
    REQUIRE(profileBytes(directory.scoreFile()) == savedBytes);
}

TEST_CASE("an abandoned best remains durable after a later lower scoring completed round",
          "[score][acceptance][lifecycle]") {
    AcceptanceDirectory directory;
    ScoreManager manager(directory.scoreFile());
    REQUIRE(manager.load());
    GameSession session(4U);
    session.setPlayerName("Abandoned player");
    ScoreCoordinator coordinator(manager);
    const auto abandonedId = session.gameId();
    for (std::size_t turn = 0;
         turn < 5000 && session.game().getScore() <= 704 && !session.game().isGameOver(); ++turn) {
        session.applyMove(kDirections[turn % kDirections.size()]);
    }
    REQUIRE_FALSE(session.game().isGameOver());
    REQUIRE(session.game().getScore() > 704);
    const auto abandonedBest = session.game().getScore();
    REQUIRE(coordinator.checkpoint(session).saveSucceeded);
    REQUIRE(manager.completedGames().empty());

    // The close/restart checkpoint occurs before the old round is replaced.
    REQUIRE_FALSE(coordinator.checkpoint(session).saveAttempted);
    session.resetGame(3U);
    session.setPlayerName("Completed player");
    REQUIRE(session.gameId() != abandonedId);
    finishRound(session);
    REQUIRE(session.game().getScore() == 704);
    const auto completedId = session.gameId();
    const auto terminalCheckpoint = coordinator.checkpoint(session);
    REQUIRE(terminalCheckpoint.roundCompleted);
    REQUIRE_FALSE(terminalCheckpoint.bestImproved);
    REQUIRE(terminalCheckpoint.saveSucceeded);
    REQUIRE(manager.bestScore() == abandonedBest);
    REQUIRE(manager.completedGames().size() == 1);
    REQUIRE(manager.completedGames().front().gameId == completedId);
    REQUIRE(manager.completedGames().front().playerName == "Completed player");

    ScoreManager reopened(directory.scoreFile());
    REQUIRE(reopened.load());
    REQUIRE(reopened.bestScore() == abandonedBest);
    REQUIRE(reopened.completedGames().size() == 1);
    REQUIRE(reopened.completedGames().front().gameId == completedId);
    REQUIRE(reopened.completedGames().front().gameId != abandonedId);
    REQUIRE(reopened.completedGames().front().score == 704);
    REQUIRE(reopened.topScores().front().score < reopened.bestScore());
    REQUIRE_FALSE(reopened.isDirty());
}

TEST_CASE("native completed history reopens in full and ranks independently of round identity",
          "[score][acceptance][lifecycle]") {
    AcceptanceDirectory directory;
    ScoreManager manager(directory.scoreFile());
    REQUIRE(manager.load());
    GameSession session(0U);
    ScoreCoordinator coordinator(manager);
    std::set<std::string> identities;
    std::vector<ScoreEntry> completed;
    constexpr std::array kExpectedScores{1360, 1252, 1500, 704, 5180, 1404};

    for (std::uint32_t seed = 0; seed < kExpectedScores.size(); ++seed) {
        session.resetGame(seed);
        session.setPlayerName(seed == 2U ? "İnci 🎮" : "Player " + std::to_string(seed));
        REQUIRE(identities.insert(session.gameId()).second);
        finishRound(session);
        REQUIRE(session.game().getScore() == kExpectedScores[seed]);
        const auto checkpoint = coordinator.checkpoint(session);
        REQUIRE(checkpoint.roundCompleted);
        REQUIRE(checkpoint.saveSucceeded);
        REQUIRE_FALSE(session.shouldRecordFinalScore());
        REQUIRE(manager.completedGames().size() == seed + 1U);
        completed.push_back(manager.completedGames().back());
        REQUIRE(completed.back().playerName == session.playerName());
        REQUIRE_FALSE(completed.back().playedAtUtc.empty());
        REQUIRE_FALSE(coordinator.checkpoint(session).saveAttempted);
    }

    ScoreManager reopened(directory.scoreFile());
    REQUIRE(reopened.load());
    REQUIRE(reopened.bestScore() == 5180);
    REQUIRE(reopened.completedGames().size() == completed.size());
    for (std::size_t index = 0; index < completed.size(); ++index) {
        requireSameEntry(reopened.completedGames()[index], completed[index]);
    }
    const auto topFive = reopened.topScores();
    REQUIRE(topFive.size() == 5);
    constexpr std::array<std::size_t, 5> kRanking{4, 2, 5, 0, 1};
    for (std::size_t index = 0; index < topFive.size(); ++index) {
        requireSameEntry(topFive[index], completed[kRanking[index]]);
    }

    const auto originalBytes = profileBytes(directory.scoreFile());
    for (const auto &entry : completed) {
        REQUIRE_FALSE(reopened.recordCompletedGame(entry.gameId, std::numeric_limits<int>::max(),
                                                   "Duplicate payload"));
    }
    REQUIRE(reopened.bestScore() == 5180);
    REQUIRE_FALSE(reopened.isDirty());
    REQUIRE(profileBytes(directory.scoreFile()) == originalBytes);

    // Identical score/name payloads from a fresh round remain distinct history entries.
    session.resetGame(0U);
    session.setPlayerName(completed.front().playerName);
    REQUIRE(identities.insert(session.gameId()).second);
    finishRound(session);
    REQUIRE(coordinator.checkpoint(session).saveSucceeded);
    REQUIRE(manager.completedGames().size() == 7);
    REQUIRE(manager.completedGames().back().score == completed.front().score);
    REQUIRE(manager.completedGames().back().gameId != completed.front().gameId);
    ScoreManager finalProfile(directory.scoreFile());
    REQUIRE(finalProfile.load());
    REQUIRE(finalProfile.completedGames().size() == 7);
    REQUIRE(finalProfile.bestScore() == 5180);
    REQUIRE(finalProfile.topScores().size() == 5);
}
