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
#include <utility>
#include <vector>

namespace {

using numtilo_core::ScoreEntry;
using numtilo_core::ScoreLoadStatus;
using numtilo_core::ScoreManager;
using Json = nlohmann::json;

class SchemaTemporaryDirectory {
  public:
    SchemaTemporaryDirectory() {
        static std::atomic<unsigned long long> nextId{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto parent = std::filesystem::temp_directory_path();
        for (int attempt = 0; attempt < 16; ++attempt) {
            path_ = parent / ("numtilo_score_schema_" + std::to_string(timestamp) + "_" +
                              std::to_string(nextId.fetch_add(1)));
            if (std::filesystem::create_directory(path_)) {
                return;
            }
        }
        throw std::runtime_error("could not create a unique schema-test directory");
    }

    ~SchemaTemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    SchemaTemporaryDirectory(const SchemaTemporaryDirectory &) = delete;
    SchemaTemporaryDirectory &operator=(const SchemaTemporaryDirectory &) = delete;

    std::filesystem::path file(const std::string &name = "scores.json") const {
        return path_ / name;
    }

  private:
    std::filesystem::path path_;
};

void writeSchemaText(const std::filesystem::path &path, const std::string &content) {
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output.is_open());
    output << content;
    output.close();
    REQUIRE_FALSE(output.fail());
}

std::string readSchemaText(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.is_open());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

Json schemaEntry(std::string id = "round-a", int score = 128, std::string name = "Ada",
                 std::string playedAt = "2026-10-06T09:00:00Z") {
    return {{"game_id", std::move(id)},
            {"score", score},
            {"player_name", std::move(name)},
            {"played_at", std::move(playedAt)}};
}

Json schemaProfile(int best = 128) {
    return {{"version", 2}, {"best_score", best}, {"completed_games", {schemaEntry()}}};
}

void requireSchemaEntries(const std::vector<ScoreEntry> &actual,
                          const std::vector<ScoreEntry> &expected) {
    REQUIRE(actual.size() == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        CAPTURE(index);
        REQUIRE(actual[index].gameId == expected[index].gameId);
        REQUIRE(actual[index].score == expected[index].score);
        REQUIRE(actual[index].playerName == expected[index].playerName);
        REQUIRE(actual[index].playedAtUtc == expected[index].playedAtUtc);
    }
}

std::vector<std::string> schemaIds(const std::vector<ScoreEntry> &entries) {
    std::vector<std::string> result;
    for (const auto &entry : entries) {
        result.push_back(entry.gameId);
    }
    return result;
}

struct SchemaSnapshot {
    explicit SchemaSnapshot(const ScoreManager &manager)
        : entries(manager.completedGames()), best(manager.bestScore()), dirty(manager.isDirty()),
          migration(manager.needsMigration()) {
    }

    void requireUnchanged(const ScoreManager &manager) const {
        requireSchemaEntries(manager.completedGames(), entries);
        REQUIRE(manager.bestScore() == best);
        REQUIRE(manager.isDirty() == dirty);
        REQUIRE(manager.needsMigration() == migration);
    }

    std::vector<ScoreEntry> entries;
    int best;
    bool dirty;
    bool migration;
};

struct InvalidSchemaFixture {
    std::string label;
    std::string text;
    ScoreLoadStatus status{ScoreLoadStatus::InvalidData};
};

// Exercise every rejected document against clean, dirty, and migration-pending
// memory. A rejected document must change only diagnostics and write permission.
void requireRejectedDocuments(const std::vector<InvalidSchemaFixture> &fixtures) {
    for (const auto &fixture : fixtures) {
        for (const int baseline : {0, 1, 2}) {
            CAPTURE(fixture.label, baseline);
            SchemaTemporaryDirectory directory;
            const auto path = directory.file();
            const std::string baselineText =
                baseline == 2
                    ? R"({"scores":[{"score":256,"played_at":"legacy date","player_name":"Legacy"}]})"
                    : schemaProfile(256).dump();
            writeSchemaText(path, baselineText);
            ScoreManager manager(path);
            REQUIRE(manager.load());
            if (baseline == 1) {
                REQUIRE(manager.updateBestScore(50000));
                REQUIRE(manager.recordCompletedGame("dirty-memory", 512, "Memory", "date"));
            }
            const SchemaSnapshot snapshot(manager);

            writeSchemaText(path, fixture.text);
            REQUIRE_FALSE(manager.load());
            REQUIRE(manager.lastLoadStatus() == fixture.status);
            snapshot.requireUnchanged(manager);
            REQUIRE(readSchemaText(path) == fixture.text);
            REQUIRE_FALSE(manager.save());
            snapshot.requireUnchanged(manager);
            REQUIRE(readSchemaText(path) == fixture.text);
            REQUIRE_FALSE(std::filesystem::exists(directory.file("scores.json.bak")));
            REQUIRE_FALSE(std::filesystem::exists(directory.file("scores.json.tmp")));

            // New in-memory records cannot re-enable writes after a failed load.
            REQUIRE(manager.updateBestScore(100000));
            REQUIRE(manager.recordCompletedGame("after-rejection", 1024, "New", "date"));
            REQUIRE_FALSE(manager.save());
            REQUIRE(manager.isDirty());
            REQUIRE(readSchemaText(path) == fixture.text);
        }
    }
}

} // namespace

TEST_CASE("missing score profiles start clean and write version two", "[score][schema]") {
    SchemaTemporaryDirectory directory;
    const auto path = directory.file();
    ScoreManager manager(path);
    REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::NotLoaded);
    REQUIRE_FALSE(manager.needsMigration());
    REQUIRE(manager.load());
    REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::Missing);
    REQUIRE(manager.bestScore() == 0);
    REQUIRE(manager.completedGames().empty());
    REQUIRE_FALSE(manager.isDirty());
    REQUIRE_FALSE(manager.needsMigration());
    REQUIRE_FALSE(std::filesystem::exists(path));

    REQUIRE(manager.save());
    const auto saved = Json::parse(readSchemaText(path));
    REQUIRE(saved.size() == 3);
    REQUIRE(saved.at("version").is_number_integer());
    REQUIRE(saved.at("version") == 2);
    REQUIRE(saved.at("best_score") == 0);
    REQUIRE(saved.at("completed_games") == Json::array());
    REQUIRE_FALSE(manager.isDirty());

    ScoreManager reloaded(path);
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.lastLoadStatus() == ScoreLoadStatus::LoadedV2);
    REQUIRE_FALSE(reloaded.needsMigration());
}

TEST_CASE("version two preserves source history and derives the top five", "[score][schema]") {
    SchemaTemporaryDirectory directory;
    Json root = schemaProfile(50000);
    root["completed_games"] = Json::array();
    const std::vector<int> scores{10, 80, 90, 80, 40, 80, 50, 20};
    for (std::size_t index = 0; index < scores.size(); ++index) {
        root["completed_games"].push_back(
            schemaEntry("id-" + std::to_string(index), scores[index], "İnci 🎮", "same date"));
    }
    const auto source = root.dump();
    writeSchemaText(directory.file(), source);
    ScoreManager manager(directory.file());
    REQUIRE(manager.load());
    REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::LoadedV2);
    REQUIRE(manager.bestScore() == 50000);
    REQUIRE_FALSE(manager.isDirty());
    REQUIRE_FALSE(manager.needsMigration());
    REQUIRE(manager.completedGames().size() == scores.size());
    for (std::size_t index = 0; index < scores.size(); ++index) {
        CAPTURE(index);
        REQUIRE(manager.completedGames()[index].gameId == "id-" + std::to_string(index));
        REQUIRE(manager.completedGames()[index].score == scores[index]);
        REQUIRE(manager.completedGames()[index].playerName == "İnci 🎮");
        REQUIRE(manager.completedGames()[index].playedAtUtc == "same date");
    }
    const auto snapshot = manager.completedGames();
    REQUIRE((schemaIds(manager.topScores()) ==
             std::vector<std::string>{"id-2", "id-1", "id-3", "id-5", "id-6"}));
    requireSchemaEntries(manager.completedGames(), snapshot);
    REQUIRE(readSchemaText(directory.file()) == source);
    REQUIRE_FALSE(std::filesystem::exists(directory.file("scores.json.bak")));
}

TEST_CASE("version two round trips a live best without any completed game", "[score][schema]") {
    SchemaTemporaryDirectory directory;
    ScoreManager manager(directory.file());
    REQUIRE(manager.updateBestScore(50000));
    REQUIRE(manager.completedGames().empty());
    REQUIRE(manager.save());
    REQUIRE_FALSE(manager.isDirty());

    ScoreManager loaded(directory.file());
    REQUIRE(loaded.load());
    REQUIRE(loaded.lastLoadStatus() == ScoreLoadStatus::LoadedV2);
    REQUIRE(loaded.bestScore() == 50000);
    REQUIRE(loaded.completedGames().empty());
    REQUIRE(loaded.topScores().empty());
    REQUIRE_FALSE(loaded.isDirty());
    REQUIRE_FALSE(loaded.needsMigration());
    REQUIRE(loaded.updateBestScore(60000));
    REQUIRE(loaded.save());
    REQUIRE_FALSE(loaded.isDirty());
    REQUIRE(manager.load());
    REQUIRE(manager.bestScore() == 60000);
}

TEST_CASE("version two round trips identities metadata and integer boundaries", "[score][schema]") {
    SchemaTemporaryDirectory directory;
    ScoreManager manager(directory.file());
    REQUIRE(manager.recordCompletedGame("round zero", 0, "", ""));
    REQUIRE(manager.recordCompletedGame("round-max", std::numeric_limits<int>::max(), "İnci 🎮",
                                        "free-form legacy timestamp"));
    const auto snapshot = manager.completedGames();
    REQUIRE(manager.save());
    const auto saved = Json::parse(readSchemaText(directory.file()));
    REQUIRE(saved.size() == 3);
    REQUIRE(saved.at("version") == 2);
    REQUIRE(saved.at("best_score") == std::numeric_limits<int>::max());
    REQUIRE(saved.at("completed_games").size() == 2);
    for (const auto &entry : saved.at("completed_games")) {
        REQUIRE(entry.size() == 4);
        REQUIRE(entry.contains("game_id"));
        REQUIRE(entry.contains("score"));
        REQUIRE(entry.contains("played_at"));
        REQUIRE(entry.contains("player_name"));
    }

    ScoreManager loaded(directory.file());
    REQUIRE(loaded.load());
    REQUIRE(loaded.bestScore() == std::numeric_limits<int>::max());
    requireSchemaEntries(loaded.completedGames(), snapshot);
    REQUIRE(loaded.completedGames().front().playerName == "Oyuncu");
    REQUIRE(loaded.completedGames().front().playedAtUtc.empty());
    REQUIRE_FALSE(loaded.recordCompletedGame("round-max", 0, "Changed", "Changed"));
    REQUIRE_FALSE(loaded.recordCompletedGame("round zero", 999, "Changed", "Changed"));
    requireSchemaEntries(loaded.completedGames(), snapshot);
    REQUIRE_FALSE(loaded.isDirty());
}

TEST_CASE("invalid version two schemas preserve all prior profile states", "[score][schema]") {
    std::vector<InvalidSchemaFixture> fixtures;
    for (const std::string text : {"null", "[]", "0", "true", "\"text\"", "{", ""}) {
        fixtures.push_back({"invalid root or syntax: " + text, text});
    }
    fixtures.push_back({"trailing object", schemaProfile().dump() + " {}"});
    fixtures.push_back({"trailing non-json", schemaProfile().dump() + " garbage"});

    for (const std::string key : {"version", "best_score", "completed_games"}) {
        auto root = schemaProfile();
        root.erase(key);
        fixtures.push_back({"missing root field " + key, root.dump()});
    }
    for (const auto &value :
         {Json(nullptr), Json(true), Json("2"), Json(2.0), Json::array(), Json::object()}) {
        auto root = schemaProfile();
        root["version"] = value;
        fixtures.push_back({"non-integer version " + value.dump(), root.dump()});
    }
    for (const auto &value : {Json(nullptr), Json(true), Json("128"), Json(128.0), Json(-1),
                              Json(2147483648ULL), Json(18446744073709551615ULL), Json::array()}) {
        for (const bool rootScore : {true, false}) {
            auto root = schemaProfile();
            if (rootScore) {
                root["best_score"] = value;
            } else {
                root["completed_games"][0]["score"] = value;
            }
            fixtures.push_back(
                {std::string(rootScore ? "best_score " : "entry score ") + value.dump(),
                 root.dump()});
        }
    }
    for (const auto &value : {Json(nullptr), Json(true), Json(0), Json("games"), Json::object()}) {
        auto root = schemaProfile();
        root["completed_games"] = value;
        fixtures.push_back({"non-array history " + value.dump(), root.dump()});
    }
    for (const auto &value : {Json(nullptr), Json(128), Json("entry"), Json::array()}) {
        auto root = schemaProfile();
        root["completed_games"].push_back(value);
        fixtures.push_back({"invalid entry after a valid entry " + value.dump(), root.dump()});
    }
    for (const std::string key : {"game_id", "score", "played_at", "player_name"}) {
        auto root = schemaProfile();
        root["completed_games"][0].erase(key);
        fixtures.push_back({"missing entry field " + key, root.dump()});
    }
    for (const std::string key : {"game_id", "played_at", "player_name"}) {
        for (const auto &value :
             {Json(nullptr), Json(0), Json(true), Json::array(), Json::object()}) {
            auto root = schemaProfile();
            root["completed_games"][0][key] = value;
            fixtures.push_back({"non-string " + key + " " + value.dump(), root.dump()});
        }
    }
    for (const std::string key : {"game_id", "player_name"}) {
        auto root = schemaProfile();
        root["completed_games"][0][key] = "";
        fixtures.push_back({"empty " + key, root.dump()});
    }
    for (const std::string key : {"scores", "top_scores", "future_field"}) {
        auto root = schemaProfile();
        root[key] = Json::array();
        fixtures.push_back({"unknown root field " + key, root.dump()});
    }
    auto unknownEntry = schemaProfile();
    unknownEntry["completed_games"][0]["seed"] = 42;
    fixtures.push_back({"unknown version two entry field", unknownEntry.dump()});
    fixtures.push_back({"best below history", schemaProfile(127).dump()});
    for (const int secondScore : {128, 64}) {
        auto root = schemaProfile();
        root["completed_games"].push_back(schemaEntry("round-a", secondScore));
        fixtures.push_back({"duplicate game identity " + std::to_string(secondScore), root.dump()});
    }
    fixtures.push_back({"score beyond uint64 parser range",
                        R"({"version":2,"best_score":18446744073709551616,"completed_games":[]})"});
    requireRejectedDocuments(fixtures);
}

TEST_CASE("unknown integer versions are distinguished from invalid data", "[score][schema]") {
    std::vector<InvalidSchemaFixture> fixtures;
    for (const std::string version : {"-9223372036854775808", "-1", "0", "1", "3", "99",
                                      "2147483648", "18446744073709551615"}) {
        fixtures.push_back(
            {"unknown version " + version,
             "{\"version\":" + version + ",\"best_score\":128,\"completed_games\":[]}",
             ScoreLoadStatus::UnsupportedVersion});
    }
    requireRejectedDocuments(fixtures);
}

TEST_CASE("duplicate json keys cannot silently replace score data", "[score][schema]") {
    const std::vector<InvalidSchemaFixture> fixtures{
        {"same root version", R"({"version":2,"version":2,"best_score":0,"completed_games":[]})"},
        {"different root versions",
         R"({"version":99,"version":2,"best_score":0,"completed_games":[]})"},
        {"escaped duplicate root version",
         R"({"version":2,"\u0076ersion":2,"best_score":0,"completed_games":[]})"},
        {"duplicate best", R"({"version":2,"best_score":100,"best_score":0,"completed_games":[]})"},
        {"duplicate history",
         R"({"version":2,"best_score":0,"completed_games":[],"completed_games":[]})"},
        {"duplicate nested score",
         R"({"version":2,"best_score":128,"completed_games":[{"game_id":"a","score":128,"score":0,"played_at":"date","player_name":"Ada"}]})"},
        {"escaped duplicate nested key",
         R"({"version":2,"best_score":128,"completed_games":[{"game_id":"a","score":128,"\u0073core":0,"played_at":"date","player_name":"Ada"}]})"},
        {"duplicate root legacy scores", R"({"scores":[],"scores":[]})"},
        {"duplicate metadata in legacy entry",
         R"({"scores":[{"score":128,"played_at":"date","player_name":"Ada","player_name":"Ece"}]})"},
        {"duplicate keys in ignored legacy metadata",
         R"({"scores":[{"score":128,"played_at":"date","seed":{"x":1,"x":2}}]})"},
    };
    requireRejectedDocuments(fixtures);
}

TEST_CASE("legacy reads prepare memory and saving backs up the original before migration",
          "[score][schema]") {
    SchemaTemporaryDirectory directory;
    const std::string source =
        R"({"scores":[{"score":0,"played_at":""},{"score":128,"played_at":"date one","player_name":""},{"score":512,"played_at":"date two","player_name":"İnci","seed":42},{"score":512,"played_at":"date two","player_name":"İnci","game_id":"ignored legacy metadata"}]})";
    writeSchemaText(directory.file(), source);
    ScoreManager manager(directory.file());
    REQUIRE(manager.load());
    REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::LegacyMigrationPending);
    REQUIRE(manager.needsMigration());
    REQUIRE(manager.isDirty());
    REQUIRE(manager.bestScore() == 512);
    REQUIRE(manager.completedGames().size() == 4);
    REQUIRE((schemaIds(manager.completedGames()) ==
             std::vector<std::string>{"legacy-1", "legacy-2", "legacy-3", "legacy-4"}));
    REQUIRE(manager.completedGames()[0].score == 0);
    REQUIRE(manager.completedGames()[0].playedAtUtc.empty());
    REQUIRE(manager.completedGames()[0].playerName == "Oyuncu");
    REQUIRE(manager.completedGames()[1].playerName == "Oyuncu");
    REQUIRE(manager.completedGames()[2].playerName == "İnci");
    REQUIRE(manager.completedGames()[3].playerName == "İnci");
    const SchemaSnapshot snapshot(manager);
    snapshot.requireUnchanged(manager);
    REQUIRE(readSchemaText(directory.file()) == source);
    REQUIRE_FALSE(std::filesystem::exists(directory.file("scores.json.bak")));
    REQUIRE_FALSE(std::filesystem::exists(directory.file("scores.json.tmp")));
    REQUIRE_FALSE(manager.recordCompletedGame("legacy-3", 99999));
    snapshot.requireUnchanged(manager);

    REQUIRE(manager.load());
    snapshot.requireUnchanged(manager);
    REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::LegacyMigrationPending);
    REQUIRE(manager.updateBestScore(50000));
    REQUIRE(manager.save());
    REQUIRE_FALSE(manager.needsMigration());
    REQUIRE_FALSE(manager.isDirty());
    REQUIRE(readSchemaText(directory.file("scores.json.bak")) == source);
    ScoreManager migrated(directory.file());
    REQUIRE(migrated.load());
    REQUIRE(migrated.lastLoadStatus() == ScoreLoadStatus::LoadedV2);
    REQUIRE(migrated.bestScore() == 50000);
    requireSchemaEntries(migrated.completedGames(), snapshot.entries);
}

TEST_CASE("invalid legacy entries reject the complete migration transaction", "[score][schema]") {
    std::vector<InvalidSchemaFixture> fixtures{
        {"empty unrecognized object", "{}"},
        {"ambiguous unsuffixed format", R"({"scores":[],"best_score":0,"completed_games":[]})"},
        {"unknown legacy root field", R"({"scores":[],"seed":42})"},
        {"legacy score container type", R"({"scores":{}})"},
        {"legacy timestamp missing", R"({"scores":[{"score":128}]})"},
        {"legacy timestamp null", R"({"scores":[{"score":128,"played_at":null}]})"},
        {"legacy score missing", R"({"scores":[{"played_at":"date"}]})"},
        {"legacy invalid second entry",
         R"({"scores":[{"score":128,"played_at":"date"},{"score":-1,"played_at":"later"}]})"},
    };
    for (const auto &value : {Json(nullptr), Json(true), Json("128"), Json(128.0), Json(-1),
                              Json(2147483648ULL), Json(18446744073709551615ULL)}) {
        fixtures.push_back({"legacy invalid score " + value.dump(),
                            Json{{"scores", {{{"score", value}, {"played_at", "date"}}}}}.dump()});
    }
    for (const auto &value : {Json(nullptr), Json(true), Json(0), Json::array(), Json::object()}) {
        fixtures.push_back(
            {"legacy non-string optional name " + value.dump(),
             Json{{"scores", {{{"score", 128}, {"played_at", "date"}, {"player_name", value}}}}}
                 .dump()});
    }
    requireRejectedDocuments(fixtures);
}

TEST_CASE("empty legacy histories still require migration without a read-time write",
          "[score][schema]") {
    SchemaTemporaryDirectory directory;
    const std::string source = R"({"scores":[]})";
    writeSchemaText(directory.file(), source);
    ScoreManager manager(directory.file());
    REQUIRE(manager.load());
    REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::LegacyMigrationPending);
    REQUIRE(manager.bestScore() == 0);
    REQUIRE(manager.completedGames().empty());
    REQUIRE(manager.topScores().empty());
    REQUIRE(manager.needsMigration());
    REQUIRE(manager.isDirty());
    REQUIRE(readSchemaText(directory.file()) == source);
    REQUIRE_FALSE(std::filesystem::exists(directory.file("scores.json.bak")));
    REQUIRE(manager.save());
    REQUIRE_FALSE(manager.needsMigration());
    REQUIRE_FALSE(manager.isDirty());
    REQUIRE(readSchemaText(directory.file("scores.json.bak")) == source);
    const auto migrated = Json::parse(readSchemaText(directory.file()));
    REQUIRE(migrated.at("version") == 2);
    REQUIRE(migrated.at("best_score") == 0);
    REQUIRE(migrated.at("completed_games").empty());
}

TEST_CASE("io errors preserve memory and block writes", "[score][schema]") {
    for (const bool migration : {false, true}) {
        CAPTURE(migration);
        SchemaTemporaryDirectory directory;
        const auto path = directory.file();
        const auto savedPath = directory.file("prior.json");
        const std::string source =
            migration ? R"({"scores":[{"score":128,"played_at":"date"}]})" : schemaProfile().dump();
        writeSchemaText(path, source);
        ScoreManager manager(path);
        REQUIRE(manager.load());
        REQUIRE(manager.updateBestScore(50000));
        const SchemaSnapshot snapshot(manager);
        std::filesystem::rename(path, savedPath);
        REQUIRE(std::filesystem::create_directory(path));

        REQUIRE_FALSE(manager.load());
        REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::IoError);
        snapshot.requireUnchanged(manager);
        REQUIRE_FALSE(manager.save());
        snapshot.requireUnchanged(manager);
        REQUIRE(std::filesystem::is_directory(path));
        REQUIRE(readSchemaText(savedPath) == source);
    }
}

TEST_CASE("a successful reload replaces migration flags and resets missing profiles",
          "[score][schema]") {
    SchemaTemporaryDirectory directory;
    const auto path = directory.file();
    writeSchemaText(path, R"({"scores":[{"score":128,"played_at":"date"}]})");
    ScoreManager manager(path);
    REQUIRE(manager.load());
    REQUIRE(manager.needsMigration());
    REQUIRE(manager.updateBestScore(50000));

    writeSchemaText(path, schemaProfile(1024).dump());
    REQUIRE(manager.load());
    REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::LoadedV2);
    REQUIRE(manager.bestScore() == 1024);
    REQUIRE(manager.completedGames().size() == 1);
    REQUIRE(manager.completedGames().front().gameId == "round-a");
    REQUIRE_FALSE(manager.needsMigration());
    REQUIRE_FALSE(manager.isDirty());
    REQUIRE(manager.save());

    REQUIRE(std::filesystem::remove(path));
    REQUIRE(manager.load());
    REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::Missing);
    REQUIRE(manager.bestScore() == 0);
    REQUIRE(manager.completedGames().empty());
    REQUIRE_FALSE(manager.needsMigration());
    REQUIRE_FALSE(manager.isDirty());
    REQUIRE(manager.save());
}

TEST_CASE("serialization failures preserve the previously saved profile", "[score][schema]") {
    SchemaTemporaryDirectory directory;
    ScoreManager manager(directory.file());
    REQUIRE(manager.recordCompletedGame("saved", 128, "Ada", "date"));
    REQUIRE(manager.save());
    const auto savedText = readSchemaText(directory.file());
    REQUIRE(manager.recordCompletedGame("invalid-encoding", 256,
                                        std::string(1, static_cast<char>(0xff)), "date"));
    const SchemaSnapshot snapshot(manager);
    REQUIRE_FALSE(manager.save());
    snapshot.requireUnchanged(manager);
    REQUIRE(manager.isDirty());
    REQUIRE(readSchemaText(directory.file()) == savedText);
}
