#include "app/SettingsStore.hpp"
#include "core/ScoreManager.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

using app::SettingsLoadStatus;
using app::SettingsStore;
using numtilo_core::FileSaveStatus;
using Json = nlohmann::json;
namespace fs = std::filesystem;

class SettingsTemporaryDirectory {
  public:
    SettingsTemporaryDirectory() {
        static std::atomic<unsigned long long> nextId{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 64; ++attempt) {
            path_ = fs::temp_directory_path() / ("numtilo_settings_" + std::to_string(stamp) + "_" +
                                                 std::to_string(nextId.fetch_add(1)));
            std::error_code error;
            if (fs::create_directory(path_, error)) {
                return;
            }
            if (error && error != std::errc::file_exists) {
                throw fs::filesystem_error("create settings test directory", path_, error);
            }
        }
        throw std::runtime_error("could not create a unique settings test directory");
    }

    ~SettingsTemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }

    SettingsTemporaryDirectory(const SettingsTemporaryDirectory &) = delete;
    SettingsTemporaryDirectory &operator=(const SettingsTemporaryDirectory &) = delete;

    fs::path file(const fs::path &name = "settings.json") const {
        return path_ / name;
    }

  private:
    fs::path path_;
};

void writeSettingsBytes(const fs::path &path, const std::string &bytes) {
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output.is_open());
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    REQUIRE_FALSE(output.fail());
}

std::string readSettingsBytes(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.is_open());
    std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    REQUIRE_FALSE(input.bad());
    return bytes;
}

std::string versionedSettings(const bool soundEnabled) {
    return Json{{"version", 1}, {"sound_enabled", soundEnabled}}.dump(2) + '\n';
}

std::string legacySettings(const bool soundEnabled) {
    // The CRLF and surrounding whitespace must survive migration byte-for-byte.
    return std::string(" \r\n{\r\n  \"sound_enabled\": ") + (soundEnabled ? "true" : "false") +
           "\r\n}\r\n \t";
}

struct SettingsWriteCall {
    fs::path path;
    std::string serialized;
    std::optional<std::string> expectedSource;
    bool backupLegacy;
};

class ControlledSettingsStorage final : public numtilo_core::FileStorage {
  public:
    FileSaveStatus failure{FileSaveStatus::Saved};
    bool throwDependencyFailure{false};
    std::vector<SettingsWriteCall> calls;

    FileSaveStatus save(const fs::path &path, const std::string &serialized,
                        const std::optional<std::string> &expectedSource,
                        const bool backupLegacy) override {
        calls.push_back({path, serialized, expectedSource, backupLegacy});
        if (throwDependencyFailure) {
            throw std::runtime_error("injected settings dependency failure");
        }
        if (failure != FileSaveStatus::Saved) {
            return failure;
        }
        return native_->save(path, serialized, expectedSource, backupLegacy);
    }

  private:
    std::shared_ptr<numtilo_core::FileStorage> native_{numtilo_core::makeFileStorage()};
};

struct SettingsSnapshot {
    explicit SettingsSnapshot(const SettingsStore &store)
        : soundEnabled(store.soundEnabled()), dirty(store.isDirty()),
          migration(store.needsMigration()) {
    }

    void requireUnchanged(const SettingsStore &store) const {
        REQUIRE(store.soundEnabled() == soundEnabled);
        REQUIRE(store.isDirty() == dirty);
        REQUIRE(store.needsMigration() == migration);
    }

    bool soundEnabled;
    bool dirty;
    bool migration;
};

void requireVersionOneFile(const fs::path &path, const bool soundEnabled) {
    const auto root = Json::parse(readSettingsBytes(path));
    REQUIRE(root.is_object());
    REQUIRE(root.size() == 2);
    REQUIRE(root.at("version") == 1);
    REQUIRE(root.at("sound_enabled").is_boolean());
    REQUIRE(root.at("sound_enabled") == soundEnabled);
}

} // namespace

TEST_CASE("settings defaults and clean saves never create profile files", "[settings][memory]") {
    SettingsTemporaryDirectory directory;
    const auto path = directory.file();
    auto storage = std::make_shared<ControlledSettingsStorage>();
    SettingsStore store(path, storage);
    REQUIRE(store.settingsFilePath() == path);
    REQUIRE(store.soundEnabled());
    REQUIRE_FALSE(store.isDirty());
    REQUIRE_FALSE(store.needsMigration());
    REQUIRE(store.lastLoadStatus() == SettingsLoadStatus::NotLoaded);
    REQUIRE(store.lastSaveStatus() == FileSaveStatus::NotSaved);
    REQUIRE_FALSE(store.setSoundEnabled(true));
    REQUIRE(store.save());
    REQUIRE(storage->calls.empty());
    REQUIRE_FALSE(fs::exists(path));

    REQUIRE(store.load());
    REQUIRE(store.lastLoadStatus() == SettingsLoadStatus::Missing);
    REQUIRE(store.soundEnabled());
    REQUIRE_FALSE(store.isDirty());
    REQUIRE(store.save());
    REQUIRE(store.lastSaveStatus() == FileSaveStatus::NotSaved);
    REQUIRE(storage->calls.empty());
    REQUIRE_FALSE(fs::exists(path));
}

TEST_CASE("sound changes retain one dirty snapshot until saved", "[settings][memory]") {
    SettingsTemporaryDirectory directory;
    auto storage = std::make_shared<ControlledSettingsStorage>();
    SettingsStore store(directory.file(), storage);
    REQUIRE(store.setSoundEnabled(false));
    REQUIRE_FALSE(store.soundEnabled());
    REQUIRE(store.isDirty());
    REQUIRE_FALSE(store.setSoundEnabled(false));
    REQUIRE(store.isDirty());
    REQUIRE(storage->calls.empty());
    REQUIRE(store.setSoundEnabled(true));
    REQUIRE(store.soundEnabled());
    REQUIRE(store.isDirty());
    REQUIRE(store.save());
    REQUIRE(store.lastSaveStatus() == FileSaveStatus::Saved);
    REQUIRE_FALSE(store.isDirty());
    REQUIRE(storage->calls.size() == 1);
    REQUIRE_FALSE(storage->calls.front().expectedSource.has_value());
    REQUIRE_FALSE(storage->calls.front().backupLegacy);
    requireVersionOneFile(directory.file(), true);

    REQUIRE_FALSE(store.setSoundEnabled(true));
    REQUIRE(store.save());
    REQUIRE(store.lastSaveStatus() == FileSaveStatus::NotSaved);
    REQUIRE(storage->calls.size() == 1);
}

TEST_CASE("strict version-one settings reopen without rewriting clean source",
          "[settings][schema]") {
    for (const bool enabled : {false, true}) {
        CAPTURE(enabled);
        SettingsTemporaryDirectory directory;
        const auto source = versionedSettings(enabled) + " \r\n";
        writeSettingsBytes(directory.file(), source);
        auto storage = std::make_shared<ControlledSettingsStorage>();
        SettingsStore store(directory.file(), storage);
        REQUIRE(store.load());
        REQUIRE(store.lastLoadStatus() == SettingsLoadStatus::LoadedV1);
        REQUIRE(store.soundEnabled() == enabled);
        REQUIRE_FALSE(store.isDirty());
        REQUIRE_FALSE(store.needsMigration());
        REQUIRE(store.save());
        REQUIRE(storage->calls.empty());
        REQUIRE(readSettingsBytes(directory.file()) == source);
        REQUIRE_FALSE(fs::exists(directory.file("settings.json.bak")));

        REQUIRE(store.setSoundEnabled(!enabled));
        REQUIRE(store.save());
        REQUIRE(storage->calls.size() == 1);
        REQUIRE(storage->calls.front().expectedSource == source);
        REQUIRE_FALSE(storage->calls.front().backupLegacy);
        SettingsStore reopened(directory.file());
        REQUIRE(reopened.load());
        REQUIRE(reopened.soundEnabled() == !enabled);
        REQUIRE(reopened.lastLoadStatus() == SettingsLoadStatus::LoadedV1);
        REQUIRE_FALSE(reopened.isDirty());
    }
}

TEST_CASE("legacy settings migrate with exact backup and latest sound preference",
          "[settings][migration]") {
    for (const bool enabled : {false, true}) {
        CAPTURE(enabled);
        SettingsTemporaryDirectory directory;
        const auto source = legacySettings(enabled);
        writeSettingsBytes(directory.file(), source);
        auto storage = std::make_shared<ControlledSettingsStorage>();
        SettingsStore store(directory.file(), storage);
        REQUIRE(store.load());
        REQUIRE(store.lastLoadStatus() == SettingsLoadStatus::LegacyMigrationPending);
        REQUIRE(store.soundEnabled() == enabled);
        REQUIRE(store.isDirty());
        REQUIRE(store.needsMigration());
        REQUIRE(readSettingsBytes(directory.file()) == source);
        REQUIRE_FALSE(fs::exists(directory.file("settings.json.bak")));
        REQUIRE(store.setSoundEnabled(!enabled));
        REQUIRE(store.save());
        REQUIRE(storage->calls.size() == 1);
        REQUIRE(storage->calls.front().expectedSource == source);
        REQUIRE(storage->calls.front().backupLegacy);
        REQUIRE(store.lastSaveStatus() == FileSaveStatus::Saved);
        REQUIRE_FALSE(store.isDirty());
        REQUIRE_FALSE(store.needsMigration());
        REQUIRE(readSettingsBytes(directory.file("settings.json.bak")) == source);
        requireVersionOneFile(directory.file(), !enabled);

        REQUIRE(store.setSoundEnabled(enabled));
        REQUIRE(store.save());
        REQUIRE(storage->calls.size() == 2);
        REQUIRE(storage->calls.back().expectedSource == storage->calls.front().serialized);
        REQUIRE_FALSE(storage->calls.back().backupLegacy);
        REQUIRE(readSettingsBytes(directory.file("settings.json.bak")) == source);
        SettingsStore reopened(directory.file());
        REQUIRE(reopened.load());
        REQUIRE(reopened.soundEnabled() == enabled);
        REQUIRE_FALSE(reopened.needsMigration());
    }
}

TEST_CASE("legacy settings without preference changes still require migration save",
          "[settings][migration]") {
    SettingsTemporaryDirectory directory;
    const auto source = legacySettings(true);
    writeSettingsBytes(directory.file(), source);
    SettingsStore store(directory.file());
    REQUIRE(store.load());
    REQUIRE_FALSE(store.setSoundEnabled(true));
    REQUIRE(store.isDirty());
    REQUIRE(store.save());
    requireVersionOneFile(directory.file(), true);
    REQUIRE(readSettingsBytes(directory.file("settings.json.bak")) == source);
}

TEST_CASE("invalid settings preserve previous memory and block all writes",
          "[settings][schema][transaction]") {
    const std::vector<std::string> invalid{
        "",
        " ",
        "{",
        "{}",
        "null",
        "[]",
        "true",
        "1",
        R"({"version":1})",
        R"({"version":true,"sound_enabled":true})",
        R"({"version":1.0,"sound_enabled":true})",
        R"({"version":"1","sound_enabled":true})",
        R"({"version":null,"sound_enabled":true})",
        R"({"version":1,"sound_enabled":1})",
        R"({"version":1,"sound_enabled":"false"})",
        R"({"version":1,"sound_enabled":null})",
        R"({"version":1,"sound_enabled":[]})",
        R"({"version":1,"sound_enabled":{}})",
        R"({"version":1,"sound_enabled":false,"extra":42})",
        R"({"sound_enabled":false,"extra":42})",
        R"({"sound_enabled":0})",
        R"({"version":1,"version":1,"sound_enabled":false})",
        R"({"version":1,"sound_enabled":true,"sound_enabled":false})",
        R"({"sound_enabled":true,"sound_enabled":false})",
        R"({"version":1,"sound_enabled":false} {})",
        R"({"version":1,"sound_enabled":false} trailing)"};
    for (const bool previousLegacy : {false, true}) {
        for (const auto &badSource : invalid) {
            CAPTURE(previousLegacy, badSource);
            SettingsTemporaryDirectory directory;
            writeSettingsBytes(directory.file(),
                               previousLegacy ? legacySettings(false) : versionedSettings(false));
            auto storage = std::make_shared<ControlledSettingsStorage>();
            SettingsStore store(directory.file(), storage);
            REQUIRE(store.load());
            REQUIRE(store.setSoundEnabled(true));
            const SettingsSnapshot snapshot(store);
            writeSettingsBytes(directory.file(), badSource);
            REQUIRE_FALSE(store.load());
            REQUIRE(store.lastLoadStatus() == SettingsLoadStatus::InvalidData);
            snapshot.requireUnchanged(store);
            REQUIRE_FALSE(store.save());
            REQUIRE(store.lastSaveStatus() == FileSaveStatus::WriteBlocked);
            REQUIRE(storage->calls.empty());
            REQUIRE(readSettingsBytes(directory.file()) == badSource);
            REQUIRE_FALSE(fs::exists(directory.file("settings.json.bak")));
        }
    }
}

TEST_CASE("unsupported integer settings versions are protected without resetting memory",
          "[settings][schema][version]") {
    for (const std::string version : {"0", "2", "99", "-1", "18446744073709551615"}) {
        CAPTURE(version);
        SettingsTemporaryDirectory directory;
        writeSettingsBytes(directory.file(), versionedSettings(false));
        auto storage = std::make_shared<ControlledSettingsStorage>();
        SettingsStore store(directory.file(), storage);
        REQUIRE(store.load());
        REQUIRE(store.setSoundEnabled(true));
        const SettingsSnapshot snapshot(store);
        const auto future = "{\"version\":" + version + ",\"sound_enabled\":false}";
        writeSettingsBytes(directory.file(), future);
        REQUIRE_FALSE(store.load());
        REQUIRE(store.lastLoadStatus() == SettingsLoadStatus::UnsupportedVersion);
        snapshot.requireUnchanged(store);
        REQUIRE_FALSE(store.save());
        REQUIRE(store.lastSaveStatus() == FileSaveStatus::WriteBlocked);
        REQUIRE(storage->calls.empty());
        REQUIRE(readSettingsBytes(directory.file()) == future);
    }
}

TEST_CASE("existing settings cannot be overwritten before successful validation",
          "[settings][schema][protection]") {
    SettingsTemporaryDirectory directory;
    const auto source = versionedSettings(true);
    writeSettingsBytes(directory.file(), source);
    auto storage = std::make_shared<ControlledSettingsStorage>();
    SettingsStore store(directory.file(), storage);
    REQUIRE(store.setSoundEnabled(false));
    const SettingsSnapshot snapshot(store);
    REQUIRE_FALSE(store.save());
    REQUIRE(store.lastSaveStatus() == FileSaveStatus::WriteBlocked);
    snapshot.requireUnchanged(store);
    REQUIRE(storage->calls.empty());
    REQUIRE(readSettingsBytes(directory.file()) == source);
    REQUIRE(store.load());
    REQUIRE(store.soundEnabled());
    REQUIRE_FALSE(store.isDirty());
    REQUIRE(store.setSoundEnabled(false));
    REQUIRE(store.save());
    requireVersionOneFile(directory.file(), false);
}

TEST_CASE("settings save failures retain the same snapshot for retry",
          "[settings][storage][retry]") {
    for (const bool legacy : {false, true}) {
        for (const auto failure : {FileSaveStatus::IoError, FileSaveStatus::BackupConflict,
                                   FileSaveStatus::SerializationError, FileSaveStatus::NotSaved}) {
            CAPTURE(legacy, static_cast<int>(failure));
            SettingsTemporaryDirectory directory;
            const auto source = legacy ? legacySettings(true) : versionedSettings(true);
            writeSettingsBytes(directory.file(), source);
            auto storage = std::make_shared<ControlledSettingsStorage>();
            storage->failure = failure;
            SettingsStore store(directory.file(), storage);
            REQUIRE(store.load());
            REQUIRE(store.setSoundEnabled(false));
            const SettingsSnapshot snapshot(store);
            REQUIRE_FALSE(store.save());
            REQUIRE(store.lastSaveStatus() == failure);
            snapshot.requireUnchanged(store);
            REQUIRE(readSettingsBytes(directory.file()) == source);
            REQUIRE_FALSE(fs::exists(directory.file("settings.json.bak")));
            REQUIRE(storage->calls.size() == 1);
            REQUIRE(storage->calls.front().expectedSource == source);
            REQUIRE(storage->calls.front().backupLegacy == legacy);

            REQUIRE_FALSE(store.setSoundEnabled(false));
            storage->failure = FileSaveStatus::Saved;
            REQUIRE(store.save());
            REQUIRE(store.lastSaveStatus() == FileSaveStatus::Saved);
            REQUIRE_FALSE(store.isDirty());
            REQUIRE_FALSE(store.needsMigration());
            REQUIRE(storage->calls.size() == 2);
            REQUIRE(storage->calls.back().serialized == storage->calls.front().serialized);
            REQUIRE(storage->calls.back().expectedSource == source);
            REQUIRE(storage->calls.back().backupLegacy == legacy);
            requireVersionOneFile(directory.file(), false);
            if (legacy) {
                REQUIRE(readSettingsBytes(directory.file("settings.json.bak")) == source);
            }
        }
    }
}

TEST_CASE("a settings storage dependency exception remains retryable",
          "[settings][storage][retry]") {
    SettingsTemporaryDirectory directory;
    auto storage = std::make_shared<ControlledSettingsStorage>();
    storage->throwDependencyFailure = true;
    SettingsStore store(directory.file(), storage);
    REQUIRE(store.setSoundEnabled(false));
    const SettingsSnapshot snapshot(store);
    REQUIRE_FALSE(store.save());
    REQUIRE(store.lastSaveStatus() == FileSaveStatus::IoError);
    snapshot.requireUnchanged(store);
    REQUIRE_FALSE(fs::exists(directory.file()));
    storage->throwDependencyFailure = false;
    REQUIRE(store.save());
    requireVersionOneFile(directory.file(), false);
}

TEST_CASE("native settings migration preserves a conflicting backup until resolved",
          "[settings][storage][migration]") {
    SettingsTemporaryDirectory directory;
    const auto source = legacySettings(false);
    const std::string unrelatedBackup = "backup from another source\r\n";
    writeSettingsBytes(directory.file(), source);
    writeSettingsBytes(directory.file("settings.json.bak"), unrelatedBackup);
    SettingsStore store(directory.file());
    REQUIRE(store.load());
    const SettingsSnapshot snapshot(store);
    REQUIRE_FALSE(store.save());
    REQUIRE(store.lastSaveStatus() == FileSaveStatus::BackupConflict);
    snapshot.requireUnchanged(store);
    REQUIRE(readSettingsBytes(directory.file()) == source);
    REQUIRE(readSettingsBytes(directory.file("settings.json.bak")) == unrelatedBackup);

    REQUIRE(fs::remove(directory.file("settings.json.bak")));
    REQUIRE(store.save());
    requireVersionOneFile(directory.file(), false);
    REQUIRE(readSettingsBytes(directory.file("settings.json.bak")) == source);
}

TEST_CASE("native settings migration can reuse an identical existing backup",
          "[settings][storage][migration]") {
    SettingsTemporaryDirectory directory;
    const auto source = legacySettings(false);
    writeSettingsBytes(directory.file(), source);
    writeSettingsBytes(directory.file("settings.json.bak"), source);
    SettingsStore store(directory.file());
    REQUIRE(store.load());
    REQUIRE(store.save());
    requireVersionOneFile(directory.file(), false);
    REQUIRE(readSettingsBytes(directory.file("settings.json.bak")) == source);
}

TEST_CASE("settings sources changed or deleted externally require revalidation before save",
          "[settings][storage][conflict]") {
    for (const bool legacy : {false, true}) {
        for (const bool deleted : {false, true}) {
            CAPTURE(legacy, deleted);
            SettingsTemporaryDirectory directory;
            const auto source = legacy ? legacySettings(true) : versionedSettings(true);
            const auto external = versionedSettings(false) + " \r\n";
            writeSettingsBytes(directory.file(), source);
            SettingsStore store(directory.file());
            REQUIRE(store.load());
            REQUIRE(store.setSoundEnabled(false));
            const SettingsSnapshot snapshot(store);
            if (deleted) {
                REQUIRE(fs::remove(directory.file()));
            } else {
                writeSettingsBytes(directory.file(), external);
            }
            REQUIRE_FALSE(store.save());
            REQUIRE(store.lastSaveStatus() == FileSaveStatus::SourceChanged);
            snapshot.requireUnchanged(store);
            if (deleted) {
                REQUIRE_FALSE(fs::exists(directory.file()));
            } else {
                REQUIRE(readSettingsBytes(directory.file()) == external);
            }
            REQUIRE_FALSE(fs::exists(directory.file("settings.json.bak")));

            writeSettingsBytes(directory.file(), source);
            REQUIRE_FALSE(store.save());
            REQUIRE(store.lastSaveStatus() == FileSaveStatus::WriteBlocked);
            snapshot.requireUnchanged(store);
            REQUIRE(store.load());
            REQUIRE(store.setSoundEnabled(false));
            REQUIRE(store.save());
            requireVersionOneFile(directory.file(), false);
        }
    }
}

TEST_CASE("an externally created settings file cannot replace an expected absent source",
          "[settings][storage][conflict]") {
    SettingsTemporaryDirectory directory;
    SettingsStore store(directory.file());
    REQUIRE(store.load());
    REQUIRE(store.setSoundEnabled(false));
    const SettingsSnapshot snapshot(store);
    const auto external = versionedSettings(true);
    writeSettingsBytes(directory.file(), external);
    REQUIRE_FALSE(store.save());
    REQUIRE(store.lastSaveStatus() == FileSaveStatus::SourceChanged);
    snapshot.requireUnchanged(store);
    REQUIRE(readSettingsBytes(directory.file()) == external);
    REQUIRE_FALSE(store.save());
    REQUIRE(store.lastSaveStatus() == FileSaveStatus::WriteBlocked);
}

TEST_CASE("ordinary settings destination errors can recover without reloading dirty memory",
          "[settings][storage][retry]") {
    SettingsTemporaryDirectory directory;
    const auto parent = directory.file("new-parent");
    const auto path = parent / "settings.json";
    SettingsStore store(path);
    REQUIRE(store.setSoundEnabled(false));
    const SettingsSnapshot snapshot(store);
    writeSettingsBytes(parent, "blocking regular file");
    REQUIRE_FALSE(store.save());
    REQUIRE(store.lastSaveStatus() == FileSaveStatus::IoError);
    snapshot.requireUnchanged(store);
    REQUIRE(readSettingsBytes(parent) == "blocking regular file");
    REQUIRE(fs::remove(parent));
    REQUIRE(store.save());
    requireVersionOneFile(path, false);
    REQUIRE_FALSE(store.isDirty());
}

TEST_CASE("settings support unicode paths independently from score persistence",
          "[settings][storage][unicode]") {
    SettingsTemporaryDirectory directory;
    const auto parent = directory.file(fs::path{u8"\u0130nci oyun tercihleri \U0001F3AE"});
    const auto settingsPath = parent / fs::path{u8"ayarlar \u015F.json"};
    const auto scoresPath = directory.file("scores.json");
    numtilo_core::ScoreManager scores(scoresPath);
    REQUIRE(scores.updateBestScore(50000));
    REQUIRE(scores.save());
    const auto scoreBytes = readSettingsBytes(scoresPath);

    SettingsStore store(settingsPath);
    REQUIRE(store.load());
    REQUIRE(store.setSoundEnabled(false));
    REQUIRE(store.save());
    SettingsStore reopened(settingsPath);
    REQUIRE(reopened.load());
    REQUIRE(reopened.lastLoadStatus() == SettingsLoadStatus::LoadedV1);
    REQUIRE_FALSE(reopened.soundEnabled());
    REQUIRE_FALSE(reopened.isDirty());
    REQUIRE(readSettingsBytes(scoresPath) == scoreBytes);
    REQUIRE_FALSE(fs::exists(directory.file("settings.json")));
    REQUIRE_FALSE(fs::exists(parent / "settings.json.tmp"));

    numtilo_core::ScoreManager reopenedScores(scoresPath);
    REQUIRE(reopenedScores.load());
    REQUIRE(reopenedScores.bestScore() == 50000);
}

TEST_CASE("nonregular settings sources preserve memory and block writes", "[settings][io]") {
    SettingsTemporaryDirectory directory;
    writeSettingsBytes(directory.file(), versionedSettings(false));
    auto storage = std::make_shared<ControlledSettingsStorage>();
    SettingsStore store(directory.file(), storage);
    REQUIRE(store.load());
    REQUIRE(store.setSoundEnabled(true));
    const SettingsSnapshot snapshot(store);
    REQUIRE(fs::remove(directory.file()));
    REQUIRE(fs::create_directory(directory.file()));
    writeSettingsBytes(directory.file() / "protected-child", "keep me");
    REQUIRE_FALSE(store.load());
    REQUIRE(store.lastLoadStatus() == SettingsLoadStatus::IoError);
    snapshot.requireUnchanged(store);
    REQUIRE_FALSE(store.save());
    REQUIRE(store.lastSaveStatus() == FileSaveStatus::WriteBlocked);
    REQUIRE(storage->calls.empty());
    REQUIRE(readSettingsBytes(directory.file() / "protected-child") == "keep me");
}

#if !defined(_WIN32)
TEST_CASE("settings loads reject normal and dangling source symlinks", "[settings][symlink]") {
    for (const bool dangling : {false, true}) {
        CAPTURE(dangling);
        SettingsTemporaryDirectory directory;
        const auto source = versionedSettings(false);
        const auto target = directory.file("target.json");
        if (!dangling) {
            writeSettingsBytes(target, source);
        }
        fs::create_symlink(target, directory.file());
        auto storage = std::make_shared<ControlledSettingsStorage>();
        SettingsStore store(directory.file(), storage);
        REQUIRE_FALSE(store.load());
        REQUIRE(store.lastLoadStatus() == SettingsLoadStatus::IoError);
        REQUIRE(store.soundEnabled());
        REQUIRE(store.setSoundEnabled(false));
        REQUIRE_FALSE(store.save());
        REQUIRE(store.lastSaveStatus() == FileSaveStatus::WriteBlocked);
        REQUIRE(storage->calls.empty());
        REQUIRE(fs::is_symlink(directory.file()));
        if (dangling) {
            REQUIRE_FALSE(fs::exists(target));
        } else {
            REQUIRE(readSettingsBytes(target) == source);
        }
    }
}

TEST_CASE("a symlink replacing validated settings cannot redirect a save",
          "[settings][storage][symlink]") {
    SettingsTemporaryDirectory directory;
    const auto source = versionedSettings(true);
    const std::string unrelated = "unrelated target bytes";
    writeSettingsBytes(directory.file(), source);
    writeSettingsBytes(directory.file("target.json"), unrelated);
    SettingsStore store(directory.file());
    REQUIRE(store.load());
    REQUIRE(store.setSoundEnabled(false));
    const SettingsSnapshot snapshot(store);
    REQUIRE(fs::remove(directory.file()));
    fs::create_symlink(directory.file("target.json"), directory.file());
    REQUIRE_FALSE(store.save());
    REQUIRE(store.lastSaveStatus() == FileSaveStatus::SourceChanged);
    snapshot.requireUnchanged(store);
    REQUIRE(fs::is_symlink(directory.file()));
    REQUIRE(readSettingsBytes(directory.file("target.json")) == unrelated);
}
#endif
