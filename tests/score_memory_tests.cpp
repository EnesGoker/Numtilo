#include "core/ScoreManager.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

using numtilo_core::ScoreEntry;
using numtilo_core::ScoreLoadStatus;
using numtilo_core::ScoreManager;

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        static std::atomic<unsigned long long> nextId{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto parent = std::filesystem::temp_directory_path();
        for (int attempt = 0; attempt < 16; ++attempt) {
            path_ = parent / ("numtilo_score_memory_" + std::to_string(timestamp) + "_" +
                              std::to_string(nextId.fetch_add(1)));
            if (std::filesystem::create_directory(path_)) {
                return;
            }
        }
        throw std::runtime_error("could not create a unique score-test directory");
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

    std::filesystem::path file(const std::string &name = "scores.json") const {
        return path_ / name;
    }

  private:
    std::filesystem::path path_;
};

void writeText(const std::filesystem::path &path, const std::string &content) {
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output.is_open());
    output << content;
    output.close();
    REQUIRE_FALSE(output.fail());
}

std::string readText(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.is_open());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::vector<std::string> gameIds(const std::vector<ScoreEntry> &entries) {
    std::vector<std::string> result;
    for (const auto &entry : entries) {
        result.push_back(entry.gameId);
    }
    return result;
}

void requireSameEntries(const std::vector<ScoreEntry> &actual,
                        const std::vector<ScoreEntry> &expected) {
    REQUIRE(actual.size() == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        CAPTURE(index);
        REQUIRE(actual[index].score == expected[index].score);
        REQUIRE(actual[index].playedAtUtc == expected[index].playedAtUtc);
        REQUIRE(actual[index].playerName == expected[index].playerName);
        REQUIRE(actual[index].gameId == expected[index].gameId);
    }
}

} // namespace

TEST_CASE("score memory starts with independent empty history and a clean zero best",
          "[score][memory]") {
    ScoreManager manager(std::filesystem::path{});

    REQUIRE(manager.bestScore() == 0);
    REQUIRE(manager.completedGames().empty());
    REQUIRE(manager.topScores().empty());
    REQUIRE_FALSE(manager.isDirty());

    REQUIRE_FALSE(manager.updateBestScore(-1));
    REQUIRE_FALSE(manager.updateBestScore(0));
    REQUIRE_FALSE(manager.recordCompletedGame("", 128));
    REQUIRE_FALSE(manager.recordCompletedGame("negative", -1));
    REQUIRE(manager.bestScore() == 0);
    REQUIRE(manager.completedGames().empty());
    REQUIRE_FALSE(manager.isDirty());
}

TEST_CASE("best score advances during a round without creating a completed game",
          "[score][memory]") {
    ScoreManager manager(std::filesystem::path{});

    REQUIRE(manager.updateBestScore(50000));
    REQUIRE(manager.bestScore() == 50000);
    REQUIRE(manager.isDirty());
    REQUIRE(manager.completedGames().empty());
    REQUIRE(manager.topScores().empty());

    for (const int candidate : {-1, 0, 49999, 50000}) {
        CAPTURE(candidate);
        REQUIRE_FALSE(manager.updateBestScore(candidate));
        REQUIRE(manager.bestScore() == 50000);
        REQUIRE(manager.completedGames().empty());
        REQUIRE(manager.isDirty());
    }

    REQUIRE(manager.updateBestScore(std::numeric_limits<int>::max()));
    REQUIRE_FALSE(manager.updateBestScore(std::numeric_limits<int>::max()));
    REQUIRE(manager.bestScore() == std::numeric_limits<int>::max());
    REQUIRE(manager.completedGames().empty());
}

TEST_CASE("completed games preserve metadata and cannot reduce an independent best",
          "[score][memory]") {
    ScoreManager manager(std::filesystem::path{});
    REQUIRE(manager.updateBestScore(50000));
    REQUIRE(manager.recordCompletedGame("first", 1024, "Deniz", "2026-10-06T09:00:00Z"));
    REQUIRE(manager.recordCompletedGame("zero", 0, "", "2026-10-06T10:00:00Z"));

    REQUIRE(manager.bestScore() == 50000);
    REQUIRE(manager.completedGames().size() == 2);
    const auto &first = manager.completedGames()[0];
    REQUIRE(first.gameId == "first");
    REQUIRE(first.score == 1024);
    REQUIRE(first.playerName == "Deniz");
    REQUIRE(first.playedAtUtc == "2026-10-06T09:00:00Z");
    REQUIRE(manager.completedGames()[1].gameId == "zero");
    REQUIRE(manager.completedGames()[1].score == 0);
    REQUIRE(manager.completedGames()[1].playerName == "Oyuncu");
    REQUIRE(manager.isDirty());

    REQUIRE(manager.recordCompletedGame("record", 60000));
    REQUIRE(manager.bestScore() == 60000);
    REQUIRE(manager.completedGames().back().playerName == "Oyuncu");
    REQUIRE_FALSE(manager.completedGames().back().playedAtUtc.empty());
}

TEST_CASE("completed game identity makes repeated terminal notifications idempotent",
          "[score][memory]") {
    ScoreManager manager(std::filesystem::path{});
    REQUIRE(manager.recordCompletedGame("round-a", 128, "Ada", "2026-10-06T09:00:00Z"));
    const auto snapshot = manager.completedGames();

    REQUIRE_FALSE(manager.recordCompletedGame("round-a", 128, "Ada", "2026-10-06T09:00:00Z"));
    REQUIRE_FALSE(
        manager.recordCompletedGame("round-a", 100000, "Changed", "2099-01-01T00:00:00Z"));
    REQUIRE(manager.bestScore() == 128);
    requireSameEntries(manager.completedGames(), snapshot);

    REQUIRE(manager.recordCompletedGame("round-b", 128, "Ada", "2026-10-06T09:00:00Z"));
    REQUIRE(manager.completedGames().size() == 2);
    REQUIRE(manager.completedGames()[0].gameId == "round-a");
    REQUIRE(manager.completedGames()[1].gameId == "round-b");
}

TEST_CASE("top five is derived without trimming or sorting the completed history",
          "[score][memory]") {
    ScoreManager manager(std::filesystem::path{});
    REQUIRE(manager.recordCompletedGame("low", 10, "Low", "2026-10-06T09:00:00Z"));
    REQUIRE(manager.recordCompletedGame("tie-old", 80, "Old", "2026-10-06T08:00:00Z"));
    REQUIRE(manager.recordCompletedGame("high", 90, "High", "2026-10-06T09:00:00Z"));
    REQUIRE(manager.recordCompletedGame("tie-first", 80, "First", "2026-10-06T10:00:00Z"));
    REQUIRE(manager.recordCompletedGame("middle", 40, "Middle", "2026-10-06T09:00:00Z"));
    REQUIRE(manager.recordCompletedGame("tie-second", 80, "Second", "2026-10-06T10:00:00Z"));
    REQUIRE(manager.recordCompletedGame("fifth", 50, "Fifth", "2026-10-06T09:00:00Z"));
    REQUIRE(manager.recordCompletedGame("last", 20, "Last", "2026-10-06T09:00:00Z"));
    const auto snapshot = manager.completedGames();

    const auto top = manager.topScores();
    REQUIRE(top.size() == ScoreManager::kTopScoreCount);
    REQUIRE((gameIds(top) ==
             std::vector<std::string>{"high", "tie-first", "tie-second", "tie-old", "fifth"}));
    REQUIRE(manager.bestScore() == 90);
    REQUIRE(manager.completedGames().size() == 8);
    requireSameEntries(manager.completedGames(), snapshot);

    auto editedView = manager.topScores();
    editedView.front().score = 1;
    editedView.front().playerName = "Changed";
    REQUIRE(manager.topScores().front().score == 90);
    REQUIRE(manager.topScores().front().playerName == "High");
    requireSameEntries(manager.completedGames(), snapshot);
}

TEST_CASE("no-op score changes and reads preserve a successfully saved clean state",
          "[score][memory][storage]") {
    TemporaryDirectory directory;
    ScoreManager manager(directory.file());
    REQUIRE(manager.recordCompletedGame("done", 256, "Ada", "2026-10-06T09:00:00Z"));
    REQUIRE(manager.isDirty());
    REQUIRE(manager.save());
    REQUIRE_FALSE(manager.isDirty());
    const auto snapshot = manager.completedGames();

    REQUIRE_FALSE(manager.updateBestScore(-1));
    REQUIRE_FALSE(manager.updateBestScore(128));
    REQUIRE_FALSE(manager.updateBestScore(256));
    REQUIRE_FALSE(manager.recordCompletedGame("done", 99999, "Changed"));
    REQUIRE_FALSE(manager.recordCompletedGame("", 99999));
    REQUIRE_FALSE(manager.recordCompletedGame("bad", -1));
    REQUIRE(manager.bestScore() == 256);
    REQUIRE(manager.topScores().size() == 1);
    REQUIRE(manager.scoreFilePath() == directory.file());
    requireSameEntries(manager.completedGames(), snapshot);
    REQUIRE_FALSE(manager.isDirty());

    REQUIRE(manager.recordCompletedGame("another", 64));
    REQUIRE(manager.isDirty());
    REQUIRE(manager.completedGames().size() == 2);
    REQUIRE(manager.bestScore() == 256);
}

TEST_CASE("version two retains all completed games and identities across a save and load",
          "[score][memory][storage][schema]") {
    TemporaryDirectory directory;
    ScoreManager manager(directory.file());
    for (int index = 0; index < 8; ++index) {
        REQUIRE(manager.recordCompletedGame("round-" + std::to_string(index), index * 128,
                                            "Player " + std::to_string(index),
                                            "2026-10-06T09:00:00Z"));
    }
    REQUIRE(manager.save());
    REQUIRE_FALSE(manager.isDirty());
    const auto saved = nlohmann::json::parse(readText(directory.file()));
    REQUIRE(saved.at("version") == 2);
    REQUIRE(saved.at("best_score") == 896);
    REQUIRE(saved.at("completed_games").size() == 8);
    REQUIRE_FALSE(saved.contains("scores"));

    ScoreManager loaded(directory.file());
    REQUIRE(loaded.load());
    REQUIRE(loaded.lastLoadStatus() == ScoreLoadStatus::LoadedV2);
    REQUIRE_FALSE(loaded.needsMigration());
    REQUIRE_FALSE(loaded.isDirty());
    REQUIRE(loaded.bestScore() == 896);
    REQUIRE(loaded.completedGames().size() == 8);
    REQUIRE(loaded.topScores().size() == 5);
    for (int index = 0; index < 8; ++index) {
        CAPTURE(index);
        const auto &entry = loaded.completedGames()[static_cast<std::size_t>(index)];
        REQUIRE(entry.score == index * 128);
        REQUIRE(entry.playerName == "Player " + std::to_string(index));
        REQUIRE(entry.playedAtUtc == "2026-10-06T09:00:00Z");
        REQUIRE(entry.gameId == "round-" + std::to_string(index));
    }
    requireSameEntries(loaded.completedGames(), manager.completedGames());
    const auto originalFile = readText(directory.file());
    REQUIRE_FALSE(loaded.recordCompletedGame("round-7", 100000, "Changed"));
    REQUIRE_FALSE(loaded.isDirty());
    REQUIRE(loaded.bestScore() == 896);
    REQUIRE(readText(directory.file()) == originalFile);
}

TEST_CASE("version two persists an independent best without completing the active game",
          "[score][memory][storage][schema]") {
    TemporaryDirectory directory;
    ScoreManager manager(directory.file());
    SECTION("without any completed games") {
    }
    SECTION("with an older lower-scoring completed game") {
        REQUIRE(manager.recordCompletedGame("old", 128, "Ada", "2026-10-06T09:00:00Z"));
        REQUIRE(manager.save());
    }
    const auto originalHistory = manager.completedGames();

    REQUIRE(manager.updateBestScore(50000));
    REQUIRE(manager.save());
    REQUIRE_FALSE(manager.isDirty());
    REQUIRE(manager.bestScore() == 50000);
    requireSameEntries(manager.completedGames(), originalHistory);
    const auto saved = nlohmann::json::parse(readText(directory.file()));
    REQUIRE(saved.at("version") == 2);
    REQUIRE(saved.at("best_score") == 50000);
    REQUIRE(saved.at("completed_games").size() == originalHistory.size());
    if (!originalHistory.empty()) {
        REQUIRE(saved.at("completed_games").at(0).at("game_id") == "old");
    }

    ScoreManager loaded(directory.file());
    REQUIRE(loaded.load());
    REQUIRE(loaded.lastLoadStatus() == ScoreLoadStatus::LoadedV2);
    REQUIRE(loaded.bestScore() == 50000);
    requireSameEntries(loaded.completedGames(), originalHistory);
    REQUIRE(loaded.topScores().size() == originalHistory.size());
    if (!originalHistory.empty()) {
        REQUIRE(loaded.topScores().front().score == 128);
    }
    REQUIRE_FALSE(loaded.isDirty());
    REQUIRE_FALSE(loaded.needsMigration());
}

TEST_CASE("a failed write keeps one in-memory completion available for a save retry",
          "[score][memory][storage]") {
    TemporaryDirectory directory;
    const auto blockedParent = directory.file("blocked-parent");
    ScoreManager manager(blockedParent / "scores.json");
    writeText(blockedParent, "a file cannot serve as a directory");
    REQUIRE(manager.recordCompletedGame("done", 256, "Ada", "2026-10-06T09:00:00Z"));
    REQUIRE_FALSE(manager.save());
    REQUIRE(manager.isDirty());
    const auto snapshot = manager.completedGames();
    REQUIRE_FALSE(manager.recordCompletedGame("done", 256));
    requireSameEntries(manager.completedGames(), snapshot);

    REQUIRE(std::filesystem::remove(blockedParent));
    REQUIRE(std::filesystem::create_directory(blockedParent));
    REQUIRE(manager.save());
    REQUIRE_FALSE(manager.isDirty());
    requireSameEntries(manager.completedGames(), snapshot);

    ScoreManager loaded(blockedParent / "scores.json");
    REQUIRE(loaded.load());
    REQUIRE(loaded.completedGames().size() == 1);
    REQUIRE(loaded.bestScore() == 256);
}

TEST_CASE("failed score loads preserve score memory and block replacing the source file",
          "[score][memory][storage]") {
    TemporaryDirectory directory;
    std::string source;
    SECTION("malformed JSON") {
        source = "{ invalid json";
    }
    SECTION("trailing garbage after a valid object") {
        source = R"({"scores":[]} trailing garbage)";
    }
    SECTION("unsupported shape") {
        source = R"({"scores":"unexpected"})";
    }
    SECTION("future version") {
        source = R"({"version":99,"scores":[]})";
    }
    SECTION("version two with legacy scores is ambiguous") {
        source = R"({"version":2,"best_score":50000,"completed_games":[],"scores":[]})";
    }
    writeText(directory.file(), source);
    ScoreManager manager(directory.file());
    REQUIRE(manager.recordCompletedGame("memory", 256, "Ada", "2026-10-06T09:00:00Z"));
    const auto snapshot = manager.completedGames();

    REQUIRE_FALSE(manager.load());
    REQUIRE(manager.bestScore() == 256);
    REQUIRE(manager.isDirty());
    requireSameEntries(manager.completedGames(), snapshot);
    REQUIRE_FALSE(manager.save());
    REQUIRE(readText(directory.file()) == source);
    REQUIRE(manager.isDirty());
    requireSameEntries(manager.completedGames(), snapshot);
}

TEST_CASE("legacy inspection precedes backup and migration of the source file",
          "[score][memory][storage][migration]") {
    TemporaryDirectory directory;
    const std::string source = R"({"scores":[{"score":512,"played_at":"2026-10-06T09:00:00Z"}]})";
    writeText(directory.file(), source);
    ScoreManager manager(directory.file());
    REQUIRE(manager.recordCompletedGame("memory", 128));

    REQUIRE_FALSE(manager.save());
    REQUIRE(readText(directory.file()) == source);
    REQUIRE(manager.isDirty());

    REQUIRE(manager.load());
    REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::LegacyMigrationPending);
    REQUIRE(manager.needsMigration());
    REQUIRE(manager.bestScore() == 512);
    REQUIRE(manager.completedGames().size() == 1);
    REQUIRE(manager.completedGames().front().gameId == "legacy-1");
    REQUIRE(manager.isDirty());
    REQUIRE(readText(directory.file()) == source);
    REQUIRE(manager.save());
    REQUIRE_FALSE(manager.isDirty());
    REQUIRE_FALSE(manager.needsMigration());
    REQUIRE(readText(directory.file("scores.json.bak")) == source);
    REQUIRE(manager.recordCompletedGame("new", 256));
    REQUIRE(manager.save());
    REQUIRE_FALSE(manager.isDirty());
    REQUIRE_FALSE(manager.needsMigration());
    REQUIRE(manager.completedGames().size() == 2);
    REQUIRE(readText(directory.file("scores.json.bak")) == source);

    ScoreManager reloaded(directory.file());
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.lastLoadStatus() == ScoreLoadStatus::LoadedV2);
    REQUIRE_FALSE(reloaded.needsMigration());
    REQUIRE(reloaded.completedGames().size() == 2);
    REQUIRE(reloaded.completedGames().front().gameId == "legacy-1");
    REQUIRE_FALSE(reloaded.recordCompletedGame("legacy-1", 9999));
    REQUIRE(reloaded.bestScore() == 512);
    REQUIRE(readText(directory.file("scores.json.bak")) == source);
}

TEST_CASE("invalid legacy entries cannot partially replace or overwrite score memory",
          "[score][memory][storage][migration]") {
    TemporaryDirectory directory;
    std::string invalidEntry;
    SECTION("negative score") {
        invalidEntry = R"({"score":-1,"played_at":"2026-10-06T10:00:00Z"})";
    }
    SECTION("score beyond int range") {
        invalidEntry = R"({"score":2147483648,"played_at":"2026-10-06T10:00:00Z"})";
    }
    SECTION("unsigned maximum") {
        invalidEntry = R"({"score":18446744073709551615,"played_at":"2026-10-06T10:00:00Z"})";
    }
    SECTION("non-integral score") {
        invalidEntry = R"({"score":3.5,"played_at":"2026-10-06T10:00:00Z"})";
    }
    SECTION("missing timestamp") {
        invalidEntry = R"({"score":256})";
    }
    const auto source =
        std::string{R"({"scores":[{"score":512,"played_at":"2026-10-06T09:00:00Z"},)"} +
        invalidEntry + "]}";
    writeText(directory.file(), source);
    ScoreManager manager(directory.file());
    REQUIRE(manager.recordCompletedGame("memory", 128, "Ada", "2026-10-06T08:00:00Z"));
    const auto snapshot = manager.completedGames();

    REQUIRE_FALSE(manager.load());
    REQUIRE(manager.bestScore() == 128);
    REQUIRE(manager.isDirty());
    requireSameEntries(manager.completedGames(), snapshot);
    REQUIRE_FALSE(manager.save());
    REQUIRE(readText(directory.file()) == source);
    requireSameEntries(manager.completedGames(), snapshot);
}
