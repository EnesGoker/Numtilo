#include "app/GameSession.hpp"
#include "app/ScoreCoordinator.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

namespace {

using app::GameSession;
using app::ScoreCoordinator;
using numtilo_core::Direction;
using numtilo_core::ScoreLoadStatus;
using numtilo_core::ScoreManager;
using numtilo_core::ScoreSaveStatus;
using Json = nlohmann::json;

constexpr std::array kDirections{Direction::Left, Direction::Up, Direction::Right, Direction::Down};

class CoordinatorTemporaryDirectory {
  public:
    CoordinatorTemporaryDirectory() {
        static std::atomic<unsigned long long> nextId{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 16; ++attempt) {
            path_ = std::filesystem::temp_directory_path() /
                    ("numtilo_score_coordinator_" + std::to_string(timestamp) + "_" +
                     std::to_string(nextId.fetch_add(1)));
            if (std::filesystem::create_directory(path_)) {
                return;
            }
        }
        throw std::runtime_error("could not create a unique coordinator-test directory");
    }

    ~CoordinatorTemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    CoordinatorTemporaryDirectory(const CoordinatorTemporaryDirectory &) = delete;
    CoordinatorTemporaryDirectory &operator=(const CoordinatorTemporaryDirectory &) = delete;

    std::filesystem::path file(const std::string &name = "scores.json") const {
        return path_ / name;
    }

  private:
    std::filesystem::path path_;
};

void writeCoordinatorBytes(const std::filesystem::path &path, const std::string &bytes) {
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output.is_open());
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    REQUIRE_FALSE(output.fail());
}

std::string readCoordinatorBytes(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.is_open());
    std::string result{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    REQUIRE_FALSE(input.bad());
    return result;
}

void advancePastScore(GameSession &session, const int score) {
    for (std::size_t turn = 0;
         turn < 5000 && session.game().getScore() <= score && !session.game().isGameOver();
         ++turn) {
        session.applyMove(kDirections[turn % kDirections.size()]);
    }
    REQUIRE(session.game().getScore() > score);
    REQUIRE_FALSE(session.game().isGameOver());
}

void finishCoordinatorRound(GameSession &session) {
    for (std::size_t turn = 0; turn < 5000 && !session.game().isGameOver(); ++turn) {
        session.applyMove(kDirections[turn % kDirections.size()]);
    }
    REQUIRE(session.game().isGameOver());
    REQUIRE(session.game().getScore() > 0);
    REQUIRE(session.shouldRecordFinalScore());
}

struct CapturedSave {
    std::filesystem::path path;
    std::string serialized;
    std::optional<std::string> expectedSource;
    bool backupLegacy;
};

class CapturingScoreStorage final : public numtilo_core::ScoreStorage {
  public:
    ScoreSaveStatus save(const std::filesystem::path &path, const std::string &serialized,
                         const std::optional<std::string> &expectedSource,
                         const bool backupLegacy) override {
        saves.push_back({path, serialized, expectedSource, backupLegacy});
        if (duringSave) {
            duringSave();
        }
        const auto index = saves.size() - 1;
        return index < outcomes.size() ? outcomes[index] : ScoreSaveStatus::Saved;
    }

    std::vector<CapturedSave> saves;
    std::vector<ScoreSaveStatus> outcomes;
    std::function<void()> duringSave;
};

} // namespace

TEST_CASE("clean score checkpoints perform no disk work or completion bookkeeping",
          "[score][coordinator]") {
    STATIC_REQUIRE(!std::is_copy_constructible_v<ScoreCoordinator>);
    STATIC_REQUIRE(!std::is_copy_assignable_v<ScoreCoordinator>);
    STATIC_REQUIRE(!std::is_move_constructible_v<ScoreCoordinator>);
    STATIC_REQUIRE(!std::is_move_assignable_v<ScoreCoordinator>);

    CoordinatorTemporaryDirectory directory;
    const auto storage = std::make_shared<CapturingScoreStorage>();
    ScoreManager manager(directory.file(), storage);
    GameSession session(9U);
    ScoreCoordinator coordinator(manager);
    for (int checkpoint = 0; checkpoint < 5; ++checkpoint) {
        const auto result = coordinator.checkpoint(session);
        REQUIRE_FALSE(result.bestImproved);
        REQUIRE_FALSE(result.firstBestOfRound);
        REQUIRE_FALSE(result.roundCompleted);
        REQUIRE_FALSE(result.saveAttempted);
        REQUIRE_FALSE(result.saveSucceeded);
        REQUIRE_FALSE(result.failureToReport.has_value());
    }
    REQUIRE(storage->saves.empty());
    REQUIRE(manager.completedGames().empty());
    REQUIRE_FALSE(manager.isDirty());
}

TEST_CASE("ongoing records are saved immediately without completing the active round",
          "[score][coordinator]") {
    CoordinatorTemporaryDirectory directory;
    const auto storage = std::make_shared<CapturingScoreStorage>();
    ScoreManager manager(directory.file(), storage);
    GameSession session(9U);
    ScoreCoordinator coordinator(manager);
    advancePastScore(session, 0);
    const auto result = coordinator.checkpoint(session);

    REQUIRE(result.bestImproved);
    REQUIRE(result.firstBestOfRound);
    REQUIRE_FALSE(result.roundCompleted);
    REQUIRE(result.saveAttempted);
    REQUIRE(result.saveSucceeded);
    REQUIRE_FALSE(result.failureToReport.has_value());
    REQUIRE(storage->saves.size() == 1);
    const auto saved = Json::parse(storage->saves.front().serialized);
    REQUIRE(saved.at("best_score") == session.game().getScore());
    REQUIRE(saved.at("completed_games").empty());
    REQUIRE(storage->saves.front().path == directory.file());
    REQUIRE_FALSE(storage->saves.front().expectedSource.has_value());
    REQUIRE_FALSE(storage->saves.front().backupLegacy);
    REQUIRE(manager.completedGames().empty());
    REQUIRE_FALSE(manager.isDirty());
    REQUIRE_FALSE(session.shouldRecordFinalScore());

    const auto unchanged = coordinator.checkpoint(session);
    REQUIRE_FALSE(unchanged.bestImproved);
    REQUIRE_FALSE(unchanged.firstBestOfRound);
    REQUIRE_FALSE(unchanged.saveAttempted);
    REQUIRE(storage->saves.size() == 1);
}

TEST_CASE("terminal best and history share one save after in-memory completion acknowledgement",
          "[score][coordinator]") {
    CoordinatorTemporaryDirectory directory;
    const auto storage = std::make_shared<CapturingScoreStorage>();
    ScoreManager manager(directory.file(), storage);
    GameSession session(9U);
    session.setPlayerName("İnci 🎮");
    ScoreCoordinator coordinator(manager);
    finishCoordinatorRound(session);
    storage->duringSave = [&] {
        REQUIRE_FALSE(session.shouldRecordFinalScore());
        REQUIRE(manager.completedGames().size() == 1);
        REQUIRE(manager.isDirty());
    };

    const auto completed = coordinator.checkpoint(session);
    REQUIRE(completed.bestImproved);
    REQUIRE(completed.firstBestOfRound);
    REQUIRE(completed.roundCompleted);
    REQUIRE(completed.saveAttempted);
    REQUIRE(completed.saveSucceeded);
    REQUIRE(storage->saves.size() == 1);
    const auto saved = Json::parse(storage->saves.front().serialized);
    REQUIRE(saved.at("best_score") == session.game().getScore());
    REQUIRE(saved.at("completed_games").size() == 1);
    const auto &entry = saved.at("completed_games").front();
    REQUIRE(entry.at("game_id") == session.gameId());
    REQUIRE(entry.at("score") == session.game().getScore());
    REQUIRE(entry.at("player_name") == session.playerName());
    REQUIRE_FALSE(entry.at("played_at").get<std::string>().empty());

    const auto repeated = coordinator.checkpoint(session);
    REQUIRE_FALSE(repeated.roundCompleted);
    REQUIRE_FALSE(repeated.firstBestOfRound);
    REQUIRE_FALSE(repeated.saveAttempted);
    REQUIRE(manager.completedGames().size() == 1);
    REQUIRE(storage->saves.size() == 1);
}

TEST_CASE("a pre-existing completion identity still acknowledges the terminal session once",
          "[score][coordinator]") {
    CoordinatorTemporaryDirectory directory;
    const auto storage = std::make_shared<CapturingScoreStorage>();
    ScoreManager manager(directory.file(), storage);
    GameSession session(9U);
    finishCoordinatorRound(session);
    REQUIRE(manager.recordCompletedGame(session.gameId(), session.game().getScore(), "Original",
                                        "2026-10-06T09:00:00Z"));
    REQUIRE(manager.save());
    session.setPlayerName("Changed");
    ScoreCoordinator coordinator(manager);

    const auto result = coordinator.checkpoint(session);
    REQUIRE(result.roundCompleted);
    REQUIRE_FALSE(result.bestImproved);
    REQUIRE_FALSE(result.saveAttempted);
    REQUIRE_FALSE(session.shouldRecordFinalScore());
    REQUIRE(manager.completedGames().size() == 1);
    REQUIRE(manager.completedGames().front().playerName == "Original");
    REQUIRE(manager.completedGames().front().playedAtUtc == "2026-10-06T09:00:00Z");
    REQUIRE_FALSE(coordinator.checkpoint(session).roundCompleted);
    REQUIRE(storage->saves.size() == 1);
}

TEST_CASE("failed terminal saves retry the original snapshot across a restart without duplicates",
          "[score][coordinator]") {
    CoordinatorTemporaryDirectory directory;
    const auto storage = std::make_shared<CapturingScoreStorage>();
    storage->outcomes = {ScoreSaveStatus::IoError, ScoreSaveStatus::IoError,
                         ScoreSaveStatus::Saved};
    ScoreManager manager(directory.file(), storage);
    GameSession session(9U);
    session.setPlayerName("Original");
    ScoreCoordinator coordinator(manager);
    finishCoordinatorRound(session);
    const auto completedId = session.gameId();
    const auto score = session.game().getScore();

    const auto failed = coordinator.checkpoint(session);
    REQUIRE(failed.roundCompleted);
    REQUIRE(failed.saveAttempted);
    REQUIRE_FALSE(failed.saveSucceeded);
    REQUIRE(failed.failureToReport == ScoreSaveStatus::IoError);
    REQUIRE_FALSE(session.shouldRecordFinalScore());
    REQUIRE(manager.isDirty());
    REQUIRE(manager.completedGames().size() == 1);
    const auto firstSnapshot = storage->saves.front().serialized;

    session.resetGame(9U);
    session.setPlayerName("New player");
    REQUIRE(session.gameId() != completedId);
    const auto retryFailed = coordinator.checkpoint(session);
    REQUIRE_FALSE(retryFailed.roundCompleted);
    REQUIRE_FALSE(retryFailed.bestImproved);
    REQUIRE_FALSE(retryFailed.saveSucceeded);
    REQUIRE_FALSE(retryFailed.failureToReport.has_value());
    REQUIRE(manager.isDirty());

    const auto retrySucceeded = coordinator.checkpoint(session);
    REQUIRE(retrySucceeded.saveSucceeded);
    REQUIRE_FALSE(retrySucceeded.roundCompleted);
    REQUIRE_FALSE(retrySucceeded.failureToReport.has_value());
    REQUIRE_FALSE(manager.isDirty());
    REQUIRE(manager.bestScore() == score);
    REQUIRE(manager.completedGames().size() == 1);
    REQUIRE(manager.completedGames().front().gameId == completedId);
    REQUIRE(manager.completedGames().front().playerName == "Original");
    REQUIRE(storage->saves.size() == 3);
    for (const auto &save : storage->saves) {
        REQUIRE(save.serialized == firstSnapshot);
        REQUIRE_FALSE(save.expectedSource.has_value());
    }
}

TEST_CASE("checkpointing an abandoned round retries its best without adding history",
          "[score][coordinator]") {
    CoordinatorTemporaryDirectory directory;
    const auto storage = std::make_shared<CapturingScoreStorage>();
    storage->outcomes = {ScoreSaveStatus::IoError, ScoreSaveStatus::Saved};
    ScoreManager manager(directory.file(), storage);
    GameSession session(9U);
    ScoreCoordinator coordinator(manager);
    advancePastScore(session, 0);
    const auto originalId = session.gameId();
    const auto score = session.game().getScore();
    REQUIRE_FALSE(coordinator.checkpoint(session).saveSucceeded);

    const auto leaving = coordinator.checkpoint(session);
    REQUIRE(leaving.saveSucceeded);
    REQUIRE_FALSE(leaving.roundCompleted);
    REQUIRE(manager.completedGames().empty());
    REQUIRE(manager.bestScore() == score);
    REQUIRE_FALSE(manager.isDirty());

    session.resetGame(9U);
    REQUIRE(session.gameId() != originalId);
    REQUIRE_FALSE(coordinator.checkpoint(session).saveAttempted);
    REQUIRE(manager.bestScore() == score);
    REQUIRE(manager.completedGames().empty());
    REQUIRE(storage->saves.size() == 2);
    REQUIRE(storage->saves[0].serialized == storage->saves[1].serialized);
}

TEST_CASE("first-best notification happens once per round while every increase persists",
          "[score][coordinator]") {
    CoordinatorTemporaryDirectory directory;
    const auto storage = std::make_shared<CapturingScoreStorage>();
    ScoreManager manager(directory.file(), storage);
    GameSession session(9U);
    ScoreCoordinator coordinator(manager);
    advancePastScore(session, 0);
    REQUIRE(coordinator.checkpoint(session).firstBestOfRound);
    const auto firstBest = manager.bestScore();
    advancePastScore(session, firstBest);
    const auto second = coordinator.checkpoint(session);
    REQUIRE(second.bestImproved);
    REQUIRE_FALSE(second.firstBestOfRound);
    REQUIRE(second.saveSucceeded);
    const auto previousSnapshot = storage->saves.back().serialized;
    const auto previousBest = manager.bestScore();

    session.resetGame(9U);
    REQUIRE_FALSE(coordinator.checkpoint(session).firstBestOfRound);
    advancePastScore(session, previousBest);
    const auto newRound = coordinator.checkpoint(session);
    REQUIRE(newRound.bestImproved);
    REQUIRE(newRound.firstBestOfRound);
    REQUIRE(newRound.saveSucceeded);
    REQUIRE(storage->saves.size() == 3);
    REQUIRE(storage->saves.back().expectedSource == previousSnapshot);
    REQUIRE(manager.completedGames().empty());
}

TEST_CASE("save warnings deduplicate failures and rearm after successful recovery",
          "[score][coordinator]") {
    CoordinatorTemporaryDirectory directory;
    const auto storage = std::make_shared<CapturingScoreStorage>();
    storage->outcomes = {ScoreSaveStatus::IoError,        ScoreSaveStatus::IoError,
                         ScoreSaveStatus::BackupConflict, ScoreSaveStatus::BackupConflict,
                         ScoreSaveStatus::Saved,          ScoreSaveStatus::IoError};
    ScoreManager manager(directory.file(), storage);
    GameSession session(9U);
    ScoreCoordinator coordinator(manager);
    advancePastScore(session, 0);
    REQUIRE(coordinator.checkpoint(session).failureToReport == ScoreSaveStatus::IoError);
    REQUIRE_FALSE(coordinator.checkpoint(session).failureToReport.has_value());
    REQUIRE(coordinator.checkpoint(session).failureToReport == ScoreSaveStatus::BackupConflict);
    REQUIRE_FALSE(coordinator.checkpoint(session).failureToReport.has_value());
    REQUIRE(coordinator.checkpoint(session).saveSucceeded);
    REQUIRE_FALSE(manager.isDirty());
    advancePastScore(session, manager.bestScore());
    const auto failedAgain = coordinator.checkpoint(session);
    REQUIRE(failedAgain.failureToReport == ScoreSaveStatus::IoError);
    REQUIRE(failedAgain.bestImproved);
    REQUIRE_FALSE(failedAgain.firstBestOfRound);
    REQUIRE(manager.isDirty());
    REQUIRE(storage->saves.size() == 6);
}

TEST_CASE("source-change warnings retain the original reason on blocked retries",
          "[score][coordinator]") {
    CoordinatorTemporaryDirectory directory;
    const auto storage = std::make_shared<CapturingScoreStorage>();
    storage->outcomes = {ScoreSaveStatus::SourceChanged};
    ScoreManager manager(directory.file(), storage);
    GameSession session(9U);
    ScoreCoordinator coordinator(manager);
    advancePastScore(session, 0);
    REQUIRE(coordinator.checkpoint(session).failureToReport == ScoreSaveStatus::SourceChanged);

    for (int retry = 0; retry < 3; ++retry) {
        const auto result = coordinator.checkpoint(session);
        REQUIRE(result.saveAttempted);
        REQUIRE_FALSE(result.saveSucceeded);
        REQUIRE_FALSE(result.failureToReport.has_value());
        REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::WriteBlocked);
        REQUIRE(manager.isDirty());
    }
    REQUIRE(storage->saves.size() == 1);
    REQUIRE(manager.completedGames().empty());
}

TEST_CASE("startup checkpoints migrate legacy profiles with an exact native backup",
          "[score][coordinator][storage]") {
    CoordinatorTemporaryDirectory directory;
    const std::string source =
        "{\r\n\"scores\":[{\"score\":512,\"played_at\":\"old date\",\"player_name\":\"Ada\"}]}\r\n";
    writeCoordinatorBytes(directory.file(), source);
    ScoreManager manager(directory.file());
    REQUIRE(manager.load());
    REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::LegacyMigrationPending);
    GameSession session(9U);
    ScoreCoordinator coordinator(manager);

    const auto startup = coordinator.checkpoint(session);
    REQUIRE_FALSE(startup.bestImproved);
    REQUIRE_FALSE(startup.firstBestOfRound);
    REQUIRE_FALSE(startup.roundCompleted);
    REQUIRE(startup.saveAttempted);
    REQUIRE(startup.saveSucceeded);
    REQUIRE_FALSE(manager.isDirty());
    REQUIRE_FALSE(manager.needsMigration());
    REQUIRE(readCoordinatorBytes(directory.file("scores.json.bak")) == source);
    const auto saved = Json::parse(readCoordinatorBytes(directory.file()));
    REQUIRE(saved.at("version") == 2);
    REQUIRE(saved.at("best_score") == 512);
    REQUIRE(saved.at("completed_games").size() == 1);
    REQUIRE(saved.at("completed_games").front().at("game_id") == "legacy-1");
    REQUIRE_FALSE(coordinator.checkpoint(session).saveAttempted);
}

TEST_CASE("unsupported profiles keep memory updates while never reaching the storage writer",
          "[score][coordinator][storage]") {
    CoordinatorTemporaryDirectory directory;
    const std::string source = R"({"version":99,"future_data":"must remain intact"})";
    writeCoordinatorBytes(directory.file(), source);
    const auto storage = std::make_shared<CapturingScoreStorage>();
    ScoreManager manager(directory.file(), storage);
    REQUIRE_FALSE(manager.load());
    REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::UnsupportedVersion);
    GameSession session(9U);
    ScoreCoordinator coordinator(manager);
    advancePastScore(session, 0);

    const auto failed = coordinator.checkpoint(session);
    REQUIRE(failed.bestImproved);
    REQUIRE(failed.saveAttempted);
    REQUIRE_FALSE(failed.saveSucceeded);
    REQUIRE(failed.failureToReport == ScoreSaveStatus::WriteBlocked);
    REQUIRE(manager.bestScore() == session.game().getScore());
    REQUIRE(manager.isDirty());
    REQUIRE_FALSE(coordinator.checkpoint(session).failureToReport.has_value());
    REQUIRE(storage->saves.empty());
    REQUIRE(readCoordinatorBytes(directory.file()) == source);
}

TEST_CASE("native mid-game checkpoints survive reload and restart with no completed-game entry",
          "[score][coordinator][storage]") {
    CoordinatorTemporaryDirectory directory;
    ScoreManager manager(directory.file());
    REQUIRE(manager.load());
    GameSession session(9U);
    ScoreCoordinator coordinator(manager);
    advancePastScore(session, 0);
    const auto score = session.game().getScore();
    REQUIRE(coordinator.checkpoint(session).saveSucceeded);
    REQUIRE_FALSE(session.game().isGameOver());
    REQUIRE(manager.completedGames().empty());

    ScoreManager reopened(directory.file());
    REQUIRE(reopened.load());
    REQUIRE(reopened.bestScore() == score);
    REQUIRE(reopened.completedGames().empty());
    REQUIRE_FALSE(reopened.isDirty());
    const auto bytes = readCoordinatorBytes(directory.file());

    REQUIRE_FALSE(coordinator.checkpoint(session).saveAttempted);
    session.resetGame(9U);
    REQUIRE_FALSE(coordinator.checkpoint(session).saveAttempted);
    REQUIRE(manager.bestScore() == score);
    REQUIRE(manager.completedGames().empty());
    REQUIRE(readCoordinatorBytes(directory.file()) == bytes);
}
