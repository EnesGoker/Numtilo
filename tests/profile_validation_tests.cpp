#include "app/SettingsStore.hpp"
#include "app/detail/SettingsValidation.hpp"
#include "core/ScoreManager.hpp"
#include "core/detail/FileSnapshot.hpp"
#include "core/detail/ProfileValidation.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;
using app::SettingsLoadStatus;
using numtilo_core::ScoreLoadStatus;
using numtilo_core::detail::FileReadStatus;

class ValidationTemporaryDirectory {
  public:
    ValidationTemporaryDirectory() {
        static std::atomic<unsigned long long> nextId{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 64; ++attempt) {
            path_ = fs::temp_directory_path() / ("numtilo_validation_" + std::to_string(stamp) +
                                                 "_" + std::to_string(nextId.fetch_add(1)));
            std::error_code error;
            if (fs::create_directory(path_, error)) {
                return;
            }
            if (error && error != std::errc::file_exists) {
                throw fs::filesystem_error("create validation test directory", path_, error);
            }
        }
        throw std::runtime_error("could not create a unique validation test directory");
    }

    ~ValidationTemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }

    ValidationTemporaryDirectory(const ValidationTemporaryDirectory &) = delete;
    ValidationTemporaryDirectory &operator=(const ValidationTemporaryDirectory &) = delete;

    fs::path file(const fs::path &name = "profile.json") const {
        return path_ / name;
    }

  private:
    fs::path path_;
};

void writeValidationBytes(const fs::path &path, const std::string &bytes) {
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output.is_open());
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    REQUIRE_FALSE(output.fail());
}

std::string readValidationBytes(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.is_open());
    std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    REQUIRE_FALSE(input.bad());
    return bytes;
}

Json validationScoreEntry(const std::string &id, const int score) {
    return {{"game_id", id},
            {"score", score},
            {"played_at", "2026-10-06T12:00:00Z"},
            {"player_name", "İnci 🎮"}};
}

std::string validationScoreProfile() {
    return Json{{"version", 2},
                {"best_score", 50000},
                {"completed_games", Json::array({validationScoreEntry("first", 512),
                                                 validationScoreEntry("second", 256)})}}
               .dump(2) +
           "\r\n \t";
}

} // namespace

TEST_CASE("native snapshots preserve exact binary bytes and survive later source changes",
          "[snapshot][profile]") {
    ValidationTemporaryDirectory directory;
    const auto path = directory.file(fs::path{u8"kay\u0131t \U0001F3AE.json"});
    // Cross multiple native read chunks, preserving CRLF and embedded NUL bytes.
    std::string source(200000, 'x');
    source.replace(64000, 6, std::string("\r\n\0abc", 6));
    writeValidationBytes(path, source);
    const auto snapshot = numtilo_core::detail::readFileSnapshot(path);
    REQUIRE(snapshot.status == FileReadStatus::Read);
    REQUIRE(snapshot.bytes == source);

    // Successful reads close the handle: the file can be removed/recreated immediately.
    REQUIRE(fs::remove(path));
    writeValidationBytes(path, "later source");
    REQUIRE(snapshot.bytes == source);
    const auto later = numtilo_core::detail::readFileSnapshot(path);
    REQUIRE(later.status == FileReadStatus::Read);
    REQUIRE(later.bytes == "later source");
}

TEST_CASE("native snapshots distinguish empty missing invalid and inaccessible locations",
          "[snapshot][profile]") {
    ValidationTemporaryDirectory directory;
    auto result = numtilo_core::detail::readFileSnapshot(directory.file());
    REQUIRE(result.status == FileReadStatus::Missing);
    REQUIRE(result.bytes.empty());
    writeValidationBytes(directory.file(), "");
    result = numtilo_core::detail::readFileSnapshot(directory.file());
    REQUIRE(result.status == FileReadStatus::Read);
    REQUIRE(result.bytes.empty());
    result = numtilo_core::detail::readFileSnapshot(directory.file("missing-parent") / "file.json");
    REQUIRE(result.status == FileReadStatus::Missing);
    REQUIRE_FALSE(fs::exists(directory.file("missing-parent")));

    REQUIRE(fs::create_directory(directory.file("a-directory")));
    writeValidationBytes(directory.file("a-directory") / "child", "protected child");
    result = numtilo_core::detail::readFileSnapshot(directory.file("a-directory"));
    REQUIRE(result.status == FileReadStatus::Invalid);
    REQUIRE(result.bytes.empty());
    REQUIRE(readValidationBytes(directory.file("a-directory") / "child") == "protected child");

    writeValidationBytes(directory.file("parent-file"), "protected parent");
    result = numtilo_core::detail::readFileSnapshot(directory.file("parent-file") / "child.json");
    REQUIRE(result.status == FileReadStatus::IoError);
    REQUIRE(result.bytes.empty());
    REQUIRE(readValidationBytes(directory.file("parent-file")) == "protected parent");
}

TEST_CASE("native snapshot paths cannot read an empty path or a prefix before embedded NUL",
          "[snapshot][profile]") {
    ValidationTemporaryDirectory directory;
    const auto path = directory.file();
    writeValidationBytes(path, "prefix file must remain protected");
    auto native = path.native();
    native.push_back(fs::path::value_type{});
    native += fs::path("ignored-suffix").native();
    for (const auto &invalid : {fs::path{}, fs::path(native)}) {
        const auto snapshot = numtilo_core::detail::readFileSnapshot(invalid);
        REQUIRE(snapshot.status == FileReadStatus::Invalid);
        REQUIRE(snapshot.bytes.empty());
    }
    REQUIRE(readValidationBytes(path) == "prefix file must remain protected");
}

TEST_CASE("pure score validation and actual loading share strict schema results",
          "[validation][score][schema]") {
    const std::vector<std::pair<std::string, ScoreLoadStatus>> cases{
        {validationScoreProfile(), ScoreLoadStatus::LoadedV2},
        {R"({"scores":[]})", ScoreLoadStatus::LegacyMigrationPending},
        {R"({"scores":[{"score":512,"played_at":"old date","seed":2}]})",
         ScoreLoadStatus::LegacyMigrationPending},
        {R"({"version":99,"best_score":0,"completed_games":[]})",
         ScoreLoadStatus::UnsupportedVersion},
        {R"({"version":2.0,"best_score":0,"completed_games":[]})", ScoreLoadStatus::InvalidData},
        {R"({"version":2,"best_score":-1,"completed_games":[]})", ScoreLoadStatus::InvalidData},
        {R"({"version":2,"best_score":2147483648,"completed_games":[]})",
         ScoreLoadStatus::InvalidData},
        {R"({"version":2,"version":2,"best_score":0,"completed_games":[]})",
         ScoreLoadStatus::InvalidData},
        {R"({"version":2,"best_score":0,"completed_games":[],"extra":true})",
         ScoreLoadStatus::InvalidData},
        {R"({"scores":[{"score":2,"played_at":"date","seed":{"a":1,"a":2}}]})",
         ScoreLoadStatus::InvalidData},
        {validationScoreProfile() + "{}", ScoreLoadStatus::InvalidData},
        {validationScoreProfile() + std::string("\0ignored", 8), ScoreLoadStatus::InvalidData},
        {validationScoreProfile() + '\0', ScoreLoadStatus::InvalidData},
        {std::string(1, '\0') + validationScoreProfile(), ScoreLoadStatus::InvalidData},
        {std::string(R"({"scores":[{"score":2,"played_at":"a)") + '\0' + R"(b"}]})",
         ScoreLoadStatus::InvalidData}};
    for (const auto &[source, expected] : cases) {
        CAPTURE(source, static_cast<int>(expected));
        REQUIRE(numtilo_core::detail::validateScoreProfile(source) == expected);
        const auto parsed = numtilo_core::detail::parseScoreProfile(source);
        REQUIRE(parsed.status == expected);
        ValidationTemporaryDirectory directory;
        writeValidationBytes(directory.file(), source);
        numtilo_core::ScoreManager manager(directory.file());
        const bool accepted = expected == ScoreLoadStatus::LoadedV2 ||
                              expected == ScoreLoadStatus::LegacyMigrationPending;
        REQUIRE(manager.load() == accepted);
        REQUIRE(manager.lastLoadStatus() == expected);
        REQUIRE(readValidationBytes(directory.file()) == source);
        REQUIRE_FALSE(fs::exists(directory.file("profile.json.bak")));
        if (accepted) {
            REQUIRE(manager.bestScore() == parsed.bestScore);
            REQUIRE(manager.completedGames().size() == parsed.completedGames.size());
            for (std::size_t index = 0; index < parsed.completedGames.size(); ++index) {
                CAPTURE(index);
                REQUIRE(manager.completedGames()[index].gameId ==
                        parsed.completedGames[index].gameId);
                REQUIRE(manager.completedGames()[index].score ==
                        parsed.completedGames[index].score);
                REQUIRE(manager.completedGames()[index].playerName ==
                        parsed.completedGames[index].playerName);
                REQUIRE(manager.completedGames()[index].playedAtUtc ==
                        parsed.completedGames[index].playedAtUtc);
            }
        } else {
            REQUIRE(parsed.bestScore == 0);
            REQUIRE(parsed.completedGames.empty());
        }
    }
}

TEST_CASE("pure settings validation and actual loading share strict schema results",
          "[validation][settings][schema]") {
    const std::vector<std::pair<std::string, SettingsLoadStatus>> cases{
        {R"({"version":1,"sound_enabled":false})", SettingsLoadStatus::LoadedV1},
        {R"({"version":1,"sound_enabled":true})", SettingsLoadStatus::LoadedV1},
        {" \r\n{\"sound_enabled\":false}\r\n", SettingsLoadStatus::LegacyMigrationPending},
        {R"({"version":99,"sound_enabled":false})", SettingsLoadStatus::UnsupportedVersion},
        {R"({"version":1.0,"sound_enabled":false})", SettingsLoadStatus::InvalidData},
        {R"({"version":1,"sound_enabled":0})", SettingsLoadStatus::InvalidData},
        {R"({"version":1,"sound_enabled":false,"extra":1})", SettingsLoadStatus::InvalidData},
        {R"({"sound_enabled":false,"sound_enabled":true})", SettingsLoadStatus::InvalidData},
        {R"({"version":1,"sound_enabled":false} {})", SettingsLoadStatus::InvalidData},
        {std::string(R"({"version":1,"sound_enabled":false})") + std::string("\0ignored", 8),
         SettingsLoadStatus::InvalidData},
        {std::string(R"({"version":1,"sound_enabled":false})") + '\0',
         SettingsLoadStatus::InvalidData},
        {std::string(1, '\0') + R"({"version":1,"sound_enabled":false})",
         SettingsLoadStatus::InvalidData},
        {std::string(R"({"version":1,)") + '\0' + R"("sound_enabled":false})",
         SettingsLoadStatus::InvalidData},
        {"", SettingsLoadStatus::InvalidData}};
    for (const auto &[source, expected] : cases) {
        CAPTURE(source, static_cast<int>(expected));
        REQUIRE(app::detail::validateSettingsProfile(source) == expected);
        const auto parsed = app::detail::parseSettingsProfile(source);
        REQUIRE(parsed.status == expected);
        ValidationTemporaryDirectory directory;
        writeValidationBytes(directory.file(), source);
        app::SettingsStore store(directory.file());
        const bool accepted = expected == SettingsLoadStatus::LoadedV1 ||
                              expected == SettingsLoadStatus::LegacyMigrationPending;
        REQUIRE(store.load() == accepted);
        REQUIRE(store.lastLoadStatus() == expected);
        REQUIRE(readValidationBytes(directory.file()) == source);
        REQUIRE_FALSE(fs::exists(directory.file("profile.json.bak")));
        if (accepted) {
            REQUIRE(store.soundEnabled() == parsed.soundEnabled);
        } else {
            REQUIRE(parsed.soundEnabled);
        }
    }
}

TEST_CASE("escaped NUL remains valid JSON metadata and survives score save and reload",
          "[validation][score][nul]") {
    ValidationTemporaryDirectory directory;
    const std::string source =
        R"({"version":2,"best_score":512,"completed_games":[{"game_id":"round","score":512,"played_at":"date\u0000tail","player_name":"Ada\u0000tail"}]})";
    REQUIRE(source.find('\0') == std::string::npos);
    REQUIRE(numtilo_core::detail::validateScoreProfile(source) == ScoreLoadStatus::LoadedV2);
    const auto parsed = numtilo_core::detail::parseScoreProfile(source);
    REQUIRE(parsed.completedGames.size() == 1);
    const std::string playerName("Ada\0tail", 8);
    const std::string playedAt("date\0tail", 9);
    REQUIRE(parsed.completedGames.front().playerName == playerName);
    REQUIRE(parsed.completedGames.front().playedAtUtc == playedAt);

    writeValidationBytes(directory.file(), source);
    numtilo_core::ScoreManager manager(directory.file());
    REQUIRE(manager.load());
    REQUIRE(manager.completedGames().front().playerName == playerName);
    REQUIRE(manager.completedGames().front().playedAtUtc == playedAt);
    REQUIRE(manager.save());
    const auto saved = readValidationBytes(directory.file());
    REQUIRE(saved.find('\0') == std::string::npos);
    REQUIRE(saved.find("\\u0000") != std::string::npos);
    numtilo_core::ScoreManager reopened(directory.file());
    REQUIRE(reopened.load());
    REQUIRE(reopened.completedGames().front().playerName == playerName);
    REQUIRE(reopened.completedGames().front().playedAtUtc == playedAt);
}

TEST_CASE("failed score validation never exposes a partially parsed history",
          "[validation][score][transaction]") {
    const auto first = validationScoreEntry("first", 512);
    const auto second = validationScoreEntry("second", 256);
    std::vector<Json> invalidProfiles;
    auto duplicateId = second;
    duplicateId["game_id"] = "first";
    invalidProfiles.push_back({{"version", 2},
                               {"best_score", 512},
                               {"completed_games", Json::array({first, duplicateId})}});
    auto negative = second;
    negative["score"] = -1;
    invalidProfiles.push_back(
        {{"version", 2}, {"best_score", 512}, {"completed_games", Json::array({first, negative})}});
    invalidProfiles.push_back(
        {{"version", 2}, {"best_score", 256}, {"completed_games", Json::array({first, second})}});
    invalidProfiles.push_back(
        {{"scores", Json::array({Json{{"score", 512}, {"played_at", "valid"}},
                                 Json{{"score", 256}, {"played_at", nullptr}}})}});
    for (const auto &source : invalidProfiles) {
        CAPTURE(source.dump());
        const auto parsed = numtilo_core::detail::parseScoreProfile(source.dump());
        REQUIRE(parsed.status == ScoreLoadStatus::InvalidData);
        REQUIRE(parsed.completedGames.empty());
        REQUIRE(parsed.bestScore == 0);
    }
}

TEST_CASE("validation applies to immutable captured bytes after the source changes",
          "[validation][snapshot][transaction]") {
    ValidationTemporaryDirectory directory;
    const auto scoresPath = directory.file("scores.json");
    const auto settingsPath = directory.file("settings.json");
    const auto scores = validationScoreProfile();
    const std::string settings = "{\"sound_enabled\":false}\r\n";
    writeValidationBytes(scoresPath, scores);
    writeValidationBytes(settingsPath, settings);
    const auto scoreSnapshot = numtilo_core::detail::readFileSnapshot(scoresPath);
    const auto settingsSnapshot = numtilo_core::detail::readFileSnapshot(settingsPath);
    REQUIRE(scoreSnapshot.status == FileReadStatus::Read);
    REQUIRE(settingsSnapshot.status == FileReadStatus::Read);
    writeValidationBytes(scoresPath, "invalid score source now");
    writeValidationBytes(settingsPath, "invalid settings source now");

    REQUIRE(numtilo_core::detail::validateScoreProfile(scoreSnapshot.bytes) ==
            ScoreLoadStatus::LoadedV2);
    REQUIRE(app::detail::validateSettingsProfile(settingsSnapshot.bytes) ==
            SettingsLoadStatus::LegacyMigrationPending);
    const auto parsedScore = numtilo_core::detail::parseScoreProfile(scoreSnapshot.bytes);
    const auto parsedSettings = app::detail::parseSettingsProfile(settingsSnapshot.bytes);
    REQUIRE(parsedScore.bestScore == 50000);
    REQUIRE(parsedScore.completedGames.size() == 2);
    REQUIRE_FALSE(parsedSettings.soundEnabled);
    numtilo_core::ScoreManager currentScore(scoresPath);
    app::SettingsStore currentSettings(settingsPath);
    REQUIRE_FALSE(currentScore.load());
    REQUIRE_FALSE(currentSettings.load());
    REQUIRE(currentScore.lastLoadStatus() == ScoreLoadStatus::InvalidData);
    REQUIRE(currentSettings.lastLoadStatus() == SettingsLoadStatus::InvalidData);
    REQUIRE(readValidationBytes(scoresPath) == "invalid score source now");
    REQUIRE(readValidationBytes(settingsPath) == "invalid settings source now");
}

#if !defined(_WIN32)
TEST_CASE("native snapshots reject an existing FIFO without opening a blocking stream",
          "[snapshot][profile][fifo]") {
    ValidationTemporaryDirectory directory;
    const auto path = directory.file("named-pipe");
    REQUIRE(::mkfifo(path.c_str(), 0600) == 0);
    const auto snapshot = numtilo_core::detail::readFileSnapshot(path);
    REQUIRE(snapshot.status == FileReadStatus::Invalid);
    REQUIRE(snapshot.bytes.empty());
    REQUIRE(fs::is_fifo(path));
}

TEST_CASE("native snapshot reads reject existing and dangling source symlinks",
          "[snapshot][profile][symlink]") {
    for (const bool dangling : {false, true}) {
        CAPTURE(dangling);
        ValidationTemporaryDirectory directory;
        const auto target = directory.file("target.json");
        const auto source = validationScoreProfile();
        if (!dangling) {
            writeValidationBytes(target, source);
        }
        fs::create_symlink(target, directory.file());
        const auto snapshot = numtilo_core::detail::readFileSnapshot(directory.file());
        REQUIRE(snapshot.status == FileReadStatus::Invalid);
        REQUIRE(snapshot.bytes.empty());
        REQUIRE(fs::is_symlink(directory.file()));
        if (dangling) {
            REQUIRE_FALSE(fs::exists(target));
        } else {
            REQUIRE(readValidationBytes(target) == source);
        }
    }
}
#endif
