#include "core/ScoreManager.hpp"
#include "core/ScoreStorage.hpp"
#include "core/detail/ScoreFileOperations.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

using numtilo_core::ScoreEntry;
using numtilo_core::ScoreLoadStatus;
using numtilo_core::ScoreManager;
using numtilo_core::ScoreSaveStatus;
using numtilo_core::detail::StorageFileOperations;
using numtilo_core::detail::StorageOutputFile;
using Json = nlohmann::json;

class StorageTemporaryDirectory {
  public:
    StorageTemporaryDirectory() {
        static std::atomic<unsigned long long> nextId{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 16; ++attempt) {
            path_ = std::filesystem::temp_directory_path() /
                    ("numtilo_score_storage_" + std::to_string(timestamp) + "_" +
                     std::to_string(nextId.fetch_add(1)));
            if (std::filesystem::create_directory(path_)) {
                return;
            }
        }
        throw std::runtime_error("could not create a unique storage-test directory");
    }

    ~StorageTemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    StorageTemporaryDirectory(const StorageTemporaryDirectory &) = delete;
    StorageTemporaryDirectory &operator=(const StorageTemporaryDirectory &) = delete;

    std::filesystem::path file(const std::string &name = "scores.json") const {
        return path_ / name;
    }

  private:
    std::filesystem::path path_;
};

void writeStorageBytes(const std::filesystem::path &path, const std::string &bytes) {
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output.is_open());
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    REQUIRE_FALSE(output.fail());
}

std::string readStorageBytes(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.is_open());
    std::string result{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    REQUIRE_FALSE(input.bad());
    return result;
}

void requireStorageEntries(const std::vector<ScoreEntry> &actual,
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

struct StorageSnapshot {
    explicit StorageSnapshot(const ScoreManager &manager)
        : entries(manager.completedGames()), best(manager.bestScore()), dirty(manager.isDirty()),
          migration(manager.needsMigration()) {
    }

    void requireUnchanged(const ScoreManager &manager) const {
        requireStorageEntries(manager.completedGames(), entries);
        REQUIRE(manager.bestScore() == best);
        REQUIRE(manager.isDirty() == dirty);
        REQUIRE(manager.needsMigration() == migration);
    }

    std::vector<ScoreEntry> entries;
    int best;
    bool dirty;
    bool migration;
};

Json storageEntry(const std::string &id, int score, const std::string &name = "İnci 🎮") {
    return {{"game_id", id},
            {"score", score},
            {"player_name", name},
            {"played_at", "2026-10-06T09:00:00Z"}};
}

std::string storageV2Source() {
    Json profile = {{"version", 2}, {"best_score", 50000}, {"completed_games", Json::array()}};
    for (int index = 0; index < 8; ++index) {
        profile["completed_games"].push_back(
            storageEntry("original-" + std::to_string(index), index * 128));
    }
    return profile.dump(2) + '\n';
}

std::string storageLegacySource() {
    Json profile = {{"scores", Json::array()}};
    for (int index = 0; index < 8; ++index) {
        profile["scores"].push_back({{"score", index * 128},
                                     {"player_name", "İnci 🎮 " + std::to_string(index)},
                                     {"played_at", "legacy date " + std::to_string(index)},
                                     {"seed", 1234 + index}});
    }
    const auto unixText = profile.dump(2);
    std::string binaryText;
    for (const char character : unixText) {
        if (character == '\n') {
            binaryText += '\r';
        }
        binaryText += character;
    }
    return binaryText + "\r\n \r\n";
}

enum class StorageFailure {
    None,
    Create,
    Write,
    ThrowAfterWrite,
    Flush,
    Close,
    BackupPublish,
    MainPublish,
};

struct StorageFaultState {
    StorageFailure failure{StorageFailure::None};
    bool failBackupOnly{false};
    bool collideFirstCreate{false};
    bool collisionIsSymlink{false};
    std::filesystem::path collisionPath;
    std::filesystem::path collisionTarget;
    std::function<void()> afterFirstClose;
    std::vector<std::filesystem::path> createdPaths;
    std::vector<std::filesystem::path> removedPaths;
    int publishCalls{0};
};

// Fail inside the real storage algorithm while retaining native files and native
// publication. A prefix write specifically models an interrupted nonempty write.
class FaultOutputFile final : public StorageOutputFile {
  public:
    FaultOutputFile(std::unique_ptr<StorageOutputFile> file,
                    std::shared_ptr<StorageFaultState> fault, bool backup)
        : file_(std::move(file)), fault_(std::move(fault)), backup_(backup) {
    }

    bool write(std::string_view text) override {
        if (fails(StorageFailure::ThrowAfterWrite)) {
            static_cast<void>(file_->write(text.substr(0, text.size() / 2)));
            throw std::runtime_error("injected write dependency failure");
        }
        if (fails(StorageFailure::Write)) {
            const auto prefix = text.substr(0, text.size() / 2);
            static_cast<void>(file_->write(prefix));
            return false;
        }
        return file_->write(text);
    }

    bool flush() override {
        const bool result = file_->flush();
        return !fails(StorageFailure::Flush) && result;
    }

    bool close() override {
        const bool result = file_->close();
        if (fault_->afterFirstClose) {
            auto callback = std::exchange(fault_->afterFirstClose, std::function<void()>{});
            callback();
        }
        return !fails(StorageFailure::Close) && result;
    }

  private:
    bool fails(StorageFailure stage) const {
        return fault_->failure == stage && (!fault_->failBackupOnly || backup_);
    }

    std::unique_ptr<StorageOutputFile> file_;
    std::shared_ptr<StorageFaultState> fault_;
    bool backup_;
};

class FaultFileOperations final : public StorageFileOperations {
  public:
    explicit FaultFileOperations(std::shared_ptr<StorageFaultState> fault)
        : native_(numtilo_core::detail::makeNativeScoreFileOperations()), fault_(std::move(fault)) {
    }

    std::unique_ptr<StorageOutputFile> createExclusive(const std::filesystem::path &path) override {
        fault_->createdPaths.push_back(path);
        const bool backup = path.filename().string().find(".bak.tmp-") != std::string::npos;
        if (fault_->collideFirstCreate && fault_->createdPaths.size() == 1) {
            fault_->collisionPath = path;
            if (fault_->collisionIsSymlink) {
                std::filesystem::create_symlink(fault_->collisionTarget, path);
            } else {
                writeStorageBytes(path, "foreign collision bytes");
            }
        }
        if (fault_->failure == StorageFailure::Create && (!fault_->failBackupOnly || backup)) {
            return nullptr;
        }
        auto file = native_->createExclusive(path);
        if (!file) {
            return nullptr;
        }
        return std::make_unique<FaultOutputFile>(std::move(file), fault_, backup);
    }

    bool publish(const std::filesystem::path &from, const std::filesystem::path &to,
                 bool replace) override {
        ++fault_->publishCalls;
        const bool backup = to.extension() == ".bak";
        if ((backup && fault_->failure == StorageFailure::BackupPublish) ||
            (!backup && fault_->failure == StorageFailure::MainPublish)) {
            return false;
        }
        return native_->publish(from, to, replace);
    }

    void remove(const std::filesystem::path &path) noexcept override {
        fault_->removedPaths.push_back(path);
        native_->remove(path);
    }

  private:
    std::shared_ptr<StorageFileOperations> native_;
    std::shared_ptr<StorageFaultState> fault_;
};

void requireCreatedTempsCleaned(const StorageFaultState &fault) {
    for (const auto &path : fault.createdPaths) {
        CAPTURE(path);
        REQUIRE_FALSE(std::filesystem::exists(path));
    }
}

void requireCommittedProfile(const ScoreManager &manager, const StorageSnapshot &snapshot) {
    REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::Saved);
    REQUIRE_FALSE(manager.isDirty());
    REQUIRE_FALSE(manager.needsMigration());
    REQUIRE(manager.bestScore() == snapshot.best);
    requireStorageEntries(manager.completedGames(), snapshot.entries);
    ScoreManager reloaded(manager.scoreFilePath());
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.lastLoadStatus() == ScoreLoadStatus::LoadedV2);
    REQUIRE(reloaded.bestScore() == snapshot.best);
    requireStorageEntries(reloaded.completedGames(), snapshot.entries);
    REQUIRE_FALSE(reloaded.isDirty());
    REQUIRE_FALSE(reloaded.needsMigration());
}

} // namespace

TEST_CASE("native score storage commits complete fresh and existing profiles",
          "[score][storage][atomic]") {
    for (const bool existing : {false, true}) {
        CAPTURE(existing);
        StorageTemporaryDirectory directory;
        const auto path = directory.file();
        if (existing) {
            writeStorageBytes(path, storageV2Source());
        }
        ScoreManager manager(path);
        REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::NotSaved);
        REQUIRE(manager.load());
        if (!existing) {
            for (int index = 0; index < 8; ++index) {
                REQUIRE(manager.recordCompletedGame("fresh-" + std::to_string(index), index * 128,
                                                    "İnci 🎮", "date"));
            }
        }
        REQUIRE(manager.updateBestScore(60000));
        REQUIRE(manager.recordCompletedGame("completed-now", 1024, "Ada", "today"));
        const StorageSnapshot snapshot(manager);
        REQUIRE(manager.save());
        requireCommittedProfile(manager, snapshot);
        REQUIRE(manager.completedGames().size() == 9);
        REQUIRE(manager.topScores().size() == 5);
        REQUIRE_FALSE(std::filesystem::exists(directory.file("scores.json.bak")));
        REQUIRE_FALSE(std::filesystem::exists(directory.file("scores.json.tmp")));

        // A committed source becomes the expected source for the next write.
        REQUIRE(manager.updateBestScore(70000));
        const StorageSnapshot secondSnapshot(manager);
        REQUIRE(manager.save());
        requireCommittedProfile(manager, secondSnapshot);
    }
}

TEST_CASE("legacy migration preserves exact binary backup and all profile metadata",
          "[score][storage][migration][atomic]") {
    StorageTemporaryDirectory directory;
    const auto source = storageLegacySource();
    writeStorageBytes(directory.file(), source);
    ScoreManager manager(directory.file());
    REQUIRE(manager.load());
    REQUIRE(manager.needsMigration());
    REQUIRE(manager.isDirty());
    REQUIRE(manager.completedGames().size() == 8);
    for (std::size_t index = 0; index < manager.completedGames().size(); ++index) {
        CAPTURE(index);
        REQUIRE(manager.completedGames()[index].gameId == "legacy-" + std::to_string(index + 1));
        REQUIRE(manager.completedGames()[index].playerName == "İnci 🎮 " + std::to_string(index));
        REQUIRE(manager.completedGames()[index].playedAtUtc ==
                "legacy date " + std::to_string(index));
    }
    REQUIRE(manager.updateBestScore(50000));
    REQUIRE(manager.recordCompletedGame("after-migration", 2048, "Enes", "new date"));
    const StorageSnapshot snapshot(manager);
    REQUIRE(manager.save());
    requireCommittedProfile(manager, snapshot);
    REQUIRE(readStorageBytes(directory.file("scores.json.bak")) == source);
    const auto committedBytes = readStorageBytes(directory.file());
    REQUIRE(committedBytes != source);
    const auto committed = Json::parse(committedBytes);
    REQUIRE(committed.at("version") == 2);
    REQUIRE(committed.at("best_score") == 50000);
    REQUIRE(committed.at("completed_games").size() == 9);
    REQUIRE_FALSE(committed.contains("scores"));

    REQUIRE(manager.updateBestScore(60000));
    REQUIRE(manager.save());
    REQUIRE(readStorageBytes(directory.file("scores.json.bak")) == source);
}

TEST_CASE("an identical migration backup remains reusable after a publication failure",
          "[score][storage][migration][retry]") {
    StorageTemporaryDirectory directory;
    const auto source = storageLegacySource();
    writeStorageBytes(directory.file(), source);
    writeStorageBytes(directory.file("scores.json.bak"), source);
    auto fault = std::make_shared<StorageFaultState>();
    fault->failure = StorageFailure::MainPublish;
    auto storage = numtilo_core::makeScoreStorage(std::make_shared<FaultFileOperations>(fault));
    ScoreManager manager(directory.file(), storage);
    REQUIRE(manager.load());
    REQUIRE(manager.recordCompletedGame("completed-once", 2048, "Ada", "date"));
    const StorageSnapshot snapshot(manager);
    REQUIRE_FALSE(manager.save());
    REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::IoError);
    snapshot.requireUnchanged(manager);
    REQUIRE(readStorageBytes(directory.file()) == source);
    REQUIRE(readStorageBytes(directory.file("scores.json.bak")) == source);
    requireCreatedTempsCleaned(*fault);

    REQUIRE_FALSE(manager.recordCompletedGame("completed-once", 100000, "Changed", "Changed"));
    snapshot.requireUnchanged(manager);
    fault->failure = StorageFailure::None;
    REQUIRE(manager.save());
    requireCommittedProfile(manager, snapshot);
    REQUIRE(readStorageBytes(directory.file("scores.json.bak")) == source);
    requireCreatedTempsCleaned(*fault);
}

TEST_CASE("a different migration backup is preserved and blocks committing the source",
          "[score][storage][migration][retry]") {
    StorageTemporaryDirectory directory;
    const auto source = storageLegacySource();
    const std::string backup = "older backup belonging to another profile\r\n";
    writeStorageBytes(directory.file(), source);
    writeStorageBytes(directory.file("scores.json.bak"), backup);
    ScoreManager manager(directory.file());
    REQUIRE(manager.load());
    REQUIRE(manager.recordCompletedGame("completed-once", 2048, "Ada", "date"));
    const StorageSnapshot snapshot(manager);
    REQUIRE_FALSE(manager.save());
    REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::BackupConflict);
    snapshot.requireUnchanged(manager);
    REQUIRE(readStorageBytes(directory.file()) == source);
    REQUIRE(readStorageBytes(directory.file("scores.json.bak")) == backup);

    // Removing the conflict enables resaving this snapshot, without recording it again.
    REQUIRE(std::filesystem::remove(directory.file("scores.json.bak")));
    REQUIRE_FALSE(manager.recordCompletedGame("completed-once", 2048));
    REQUIRE(manager.save());
    requireCommittedProfile(manager, snapshot);
    REQUIRE(readStorageBytes(directory.file("scores.json.bak")) == source);
}

TEST_CASE("native write failures preserve source memory and permit duplicate-free retry",
          "[score][storage][atomic][retry]") {
    const std::vector<StorageFailure> failures{
        StorageFailure::Create,     StorageFailure::Write, StorageFailure::ThrowAfterWrite,
        StorageFailure::Flush,      StorageFailure::Close, StorageFailure::BackupPublish,
        StorageFailure::MainPublish};
    for (const bool legacy : {false, true}) {
        for (const auto failure : failures) {
            if (!legacy && failure == StorageFailure::BackupPublish) {
                continue;
            }
            CAPTURE(legacy, static_cast<int>(failure));
            StorageTemporaryDirectory directory;
            const auto source = legacy ? storageLegacySource() : storageV2Source();
            writeStorageBytes(directory.file(), source);
            auto fault = std::make_shared<StorageFaultState>();
            fault->failure = failure;
            auto storage =
                numtilo_core::makeScoreStorage(std::make_shared<FaultFileOperations>(fault));
            ScoreManager manager(directory.file(), storage);
            REQUIRE(manager.load());
            REQUIRE(manager.updateBestScore(60000));
            REQUIRE(manager.recordCompletedGame("completed-once", 2048, "Ada", "today"));
            const StorageSnapshot snapshot(manager);

            REQUIRE_FALSE(manager.save());
            REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::IoError);
            snapshot.requireUnchanged(manager);
            REQUIRE(readStorageBytes(directory.file()) == source);
            requireCreatedTempsCleaned(*fault);
            const auto backupPath = directory.file("scores.json.bak");
            if (std::filesystem::exists(backupPath)) {
                REQUIRE(legacy);
                REQUIRE(readStorageBytes(backupPath) == source);
            }

            REQUIRE_FALSE(manager.recordCompletedGame("completed-once", 100000, "Changed"));
            snapshot.requireUnchanged(manager);
            fault->failure = StorageFailure::None;
            REQUIRE(manager.save());
            requireCommittedProfile(manager, snapshot);
            REQUIRE(manager.completedGames().size() == 9);
            if (legacy) {
                REQUIRE(readStorageBytes(backupPath) == source);
            } else {
                REQUIRE_FALSE(std::filesystem::exists(backupPath));
            }
            requireCreatedTempsCleaned(*fault);
        }
    }
}

TEST_CASE("a failed fresh save leaves no partial profile before retry",
          "[score][storage][atomic][retry]") {
    for (const auto failure :
         {StorageFailure::Write, StorageFailure::ThrowAfterWrite, StorageFailure::Flush,
          StorageFailure::Close, StorageFailure::MainPublish}) {
        CAPTURE(static_cast<int>(failure));
        StorageTemporaryDirectory directory;
        auto fault = std::make_shared<StorageFaultState>();
        fault->failure = failure;
        auto storage = numtilo_core::makeScoreStorage(std::make_shared<FaultFileOperations>(fault));
        ScoreManager manager(directory.file(), storage);
        REQUIRE(manager.recordCompletedGame("fresh-round", 512, "Ada", "date"));
        const StorageSnapshot snapshot(manager);
        REQUIRE_FALSE(manager.save());
        REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::IoError);
        snapshot.requireUnchanged(manager);
        REQUIRE_FALSE(std::filesystem::exists(directory.file()));
        requireCreatedTempsCleaned(*fault);

        fault->failure = StorageFailure::None;
        REQUIRE(manager.save());
        requireCommittedProfile(manager, snapshot);
        requireCreatedTempsCleaned(*fault);
    }
}

TEST_CASE("failed backup writes leave the legacy source intact and support retry",
          "[score][storage][migration][retry]") {
    for (const auto failure :
         {StorageFailure::Create, StorageFailure::Write, StorageFailure::ThrowAfterWrite,
          StorageFailure::Flush, StorageFailure::Close}) {
        CAPTURE(static_cast<int>(failure));
        StorageTemporaryDirectory directory;
        const auto source = storageLegacySource();
        writeStorageBytes(directory.file(), source);
        auto fault = std::make_shared<StorageFaultState>();
        fault->failure = failure;
        fault->failBackupOnly = true;
        auto storage = numtilo_core::makeScoreStorage(std::make_shared<FaultFileOperations>(fault));
        ScoreManager manager(directory.file(), storage);
        REQUIRE(manager.load());
        REQUIRE(manager.recordCompletedGame("completed-once", 2048, "Ada", "date"));
        const StorageSnapshot snapshot(manager);
        REQUIRE_FALSE(manager.save());
        REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::IoError);
        snapshot.requireUnchanged(manager);
        REQUIRE(readStorageBytes(directory.file()) == source);
        REQUIRE_FALSE(std::filesystem::exists(directory.file("scores.json.bak")));
        requireCreatedTempsCleaned(*fault);

        fault->failure = StorageFailure::None;
        REQUIRE_FALSE(manager.recordCompletedGame("completed-once", 2048));
        REQUIRE(manager.save());
        requireCommittedProfile(manager, snapshot);
        REQUIRE(readStorageBytes(directory.file("scores.json.bak")) == source);
        requireCreatedTempsCleaned(*fault);
    }
}

TEST_CASE("unknown score versions cannot reach the storage writer", "[score][storage][schema]") {
    StorageTemporaryDirectory directory;
    const std::string source = R"({"version":99,"best_score":50000,"completed_games":[]})";
    writeStorageBytes(directory.file(), source);
    auto fault = std::make_shared<StorageFaultState>();
    auto storage = numtilo_core::makeScoreStorage(std::make_shared<FaultFileOperations>(fault));
    ScoreManager manager(directory.file(), storage);
    REQUIRE_FALSE(manager.load());
    REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::UnsupportedVersion);
    REQUIRE(manager.recordCompletedGame("memory", 1024));
    const StorageSnapshot snapshot(manager);
    REQUIRE_FALSE(manager.save());
    REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::WriteBlocked);
    snapshot.requireUnchanged(manager);
    REQUIRE(fault->createdPaths.empty());
    REQUIRE(fault->publishCalls == 0);
    REQUIRE(readStorageBytes(directory.file()) == source);
    REQUIRE_FALSE(std::filesystem::exists(directory.file("scores.json.bak")));
}

TEST_CASE("loaded score sources changed or deleted externally are never overwritten",
          "[score][storage][conflict]") {
    for (const bool legacy : {false, true}) {
        for (const bool deleted : {false, true}) {
            CAPTURE(legacy, deleted);
            StorageTemporaryDirectory directory;
            const auto source = legacy ? storageLegacySource() : storageV2Source();
            writeStorageBytes(directory.file(), source);
            ScoreManager manager(directory.file());
            REQUIRE(manager.load());
            REQUIRE(manager.recordCompletedGame("memory", 2048, "Ada", "date"));
            const StorageSnapshot snapshot(manager);
            const std::string external = storageV2Source() + " \n";
            if (deleted) {
                REQUIRE(std::filesystem::remove(directory.file()));
            } else {
                writeStorageBytes(directory.file(), external);
            }
            REQUIRE_FALSE(manager.save());
            REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::SourceChanged);
            snapshot.requireUnchanged(manager);
            if (deleted) {
                REQUIRE_FALSE(std::filesystem::exists(directory.file()));
            } else {
                REQUIRE(readStorageBytes(directory.file()) == external);
            }
            REQUIRE_FALSE(std::filesystem::exists(directory.file("scores.json.bak")));

            // A conflict requires explicit reinspection before writes are enabled again.
            writeStorageBytes(directory.file(), source);
            REQUIRE_FALSE(manager.save());
            REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::WriteBlocked);
            snapshot.requireUnchanged(manager);
            REQUIRE(manager.load());
            REQUIRE(manager.recordCompletedGame("memory", 2048, "Ada", "date"));
            REQUIRE(manager.save());
            requireCommittedProfile(manager, snapshot);
        }
    }
}

TEST_CASE("an externally created profile cannot replace an expected missing source",
          "[score][storage][conflict]") {
    for (const bool callLoad : {false, true}) {
        CAPTURE(callLoad);
        StorageTemporaryDirectory directory;
        ScoreManager manager(directory.file());
        if (callLoad) {
            REQUIRE(manager.load());
            REQUIRE(manager.lastLoadStatus() == ScoreLoadStatus::Missing);
        }
        REQUIRE(manager.recordCompletedGame("memory", 1024, "Ada", "date"));
        const StorageSnapshot snapshot(manager);
        const auto external = storageV2Source();
        writeStorageBytes(directory.file(), external);
        REQUIRE_FALSE(manager.save());
        REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::SourceChanged);
        snapshot.requireUnchanged(manager);
        REQUIRE(readStorageBytes(directory.file()) == external);
        REQUIRE_FALSE(std::filesystem::exists(directory.file("scores.json.bak")));
    }
}

TEST_CASE("a source changed during temporary-file preparation is checked again before commit",
          "[score][storage][atomic][conflict]") {
    for (const int sourceKind : {0, 1, 2}) {
        CAPTURE(sourceKind);
        StorageTemporaryDirectory directory;
        const bool legacy = sourceKind == 2;
        const auto source = legacy ? storageLegacySource() : storageV2Source();
        if (sourceKind != 0) {
            writeStorageBytes(directory.file(), source);
        }
        const auto external = storageV2Source() + " \n";
        auto fault = std::make_shared<StorageFaultState>();
        fault->afterFirstClose = [&directory, &external] {
            writeStorageBytes(directory.file(), external);
        };
        auto storage = numtilo_core::makeScoreStorage(std::make_shared<FaultFileOperations>(fault));
        ScoreManager manager(directory.file(), storage);
        REQUIRE(manager.load());
        REQUIRE(manager.recordCompletedGame("memory", 2048, "Ada", "date"));
        const StorageSnapshot snapshot(manager);
        REQUIRE_FALSE(manager.save());
        REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::SourceChanged);
        snapshot.requireUnchanged(manager);
        REQUIRE(readStorageBytes(directory.file()) == external);
        requireCreatedTempsCleaned(*fault);
        REQUIRE_FALSE(manager.save());
        REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::WriteBlocked);
        snapshot.requireUnchanged(manager);
        const auto backup = directory.file("scores.json.bak");
        if (std::filesystem::exists(backup)) {
            REQUIRE(legacy);
            REQUIRE(readStorageBytes(backup) == source);
        }
    }
}

TEST_CASE("serialization failures preserve source and never create a temporary file",
          "[score][storage][atomic]") {
    StorageTemporaryDirectory directory;
    const auto source = storageV2Source();
    writeStorageBytes(directory.file(), source);
    auto fault = std::make_shared<StorageFaultState>();
    auto storage = numtilo_core::makeScoreStorage(std::make_shared<FaultFileOperations>(fault));
    ScoreManager manager(directory.file(), storage);
    REQUIRE(manager.load());
    const std::string invalidUtf8(1, static_cast<char>(0xff));
    REQUIRE(manager.recordCompletedGame("invalid-utf8", 1024, invalidUtf8, "date"));
    const StorageSnapshot snapshot(manager);
    REQUIRE_FALSE(manager.save());
    REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::SerializationError);
    snapshot.requireUnchanged(manager);
    REQUIRE(readStorageBytes(directory.file()) == source);
    REQUIRE(fault->createdPaths.empty());
    REQUIRE(fault->publishCalls == 0);
}

TEST_CASE("unowned stale temporary files survive save and failure cleanup",
          "[score][storage][atomic]") {
    StorageTemporaryDirectory directory;
    const auto source = storageV2Source();
    const std::string stale = "unfinished data from another run\r\n";
    writeStorageBytes(directory.file(), source);
    writeStorageBytes(directory.file("scores.json.tmp"), stale);
    auto fault = std::make_shared<StorageFaultState>();
    fault->failure = StorageFailure::Write;
    auto storage = numtilo_core::makeScoreStorage(std::make_shared<FaultFileOperations>(fault));
    ScoreManager manager(directory.file(), storage);
    REQUIRE(manager.load());
    REQUIRE(manager.recordCompletedGame("memory", 2048, "Ada", "date"));
    const StorageSnapshot snapshot(manager);
    REQUIRE_FALSE(manager.save());
    REQUIRE(readStorageBytes(directory.file()) == source);
    REQUIRE(readStorageBytes(directory.file("scores.json.tmp")) == stale);
    requireCreatedTempsCleaned(*fault);
    fault->failure = StorageFailure::None;
    REQUIRE(manager.save());
    requireCommittedProfile(manager, snapshot);
    REQUIRE(readStorageBytes(directory.file("scores.json.tmp")) == stale);
    for (const auto &path : fault->createdPaths) {
        REQUIRE(path != directory.file("scores.json.tmp"));
    }
    requireCreatedTempsCleaned(*fault);
}

TEST_CASE("score storage supports utf8 paths and creates the destination parent",
          "[score][storage][atomic]") {
    StorageTemporaryDirectory directory;
    const auto path = directory.file("unicode-parent") /
                      std::filesystem::path{u8"\u0130nci oyun kay\u0131tlar\u0131"} /
                      std::filesystem::path{u8"rekor \U0001F3AE.json"};
    ScoreManager manager(path);
    REQUIRE(manager.recordCompletedGame("round-utf8", 512, "İnci 🎮", "date"));
    const StorageSnapshot snapshot(manager);
    REQUIRE(manager.save());
    requireCommittedProfile(manager, snapshot);
    REQUIRE(std::filesystem::is_regular_file(path));
    auto backupPath = path;
    backupPath += ".bak";
    REQUIRE_FALSE(std::filesystem::exists(backupPath));
}

TEST_CASE("exclusive creation never overwrites or removes a colliding temporary path",
          "[score][storage][atomic]") {
    std::vector<bool> symlinkCollision{false};
#if !defined(_WIN32)
    symlinkCollision.push_back(true);
#endif
    for (const bool symlink : symlinkCollision) {
        CAPTURE(symlink);
        StorageTemporaryDirectory directory;
        auto fault = std::make_shared<StorageFaultState>();
        fault->collideFirstCreate = true;
        fault->collisionIsSymlink = symlink;
        fault->collisionTarget = directory.file("unrelated-target");
        writeStorageBytes(fault->collisionTarget, "foreign collision bytes");
        auto storage = numtilo_core::makeScoreStorage(std::make_shared<FaultFileOperations>(fault));
        ScoreManager manager(directory.file(), storage);
        REQUIRE(manager.recordCompletedGame("memory", 2048, "Ada", "date"));
        const StorageSnapshot snapshot(manager);
        REQUIRE(manager.save());
        requireCommittedProfile(manager, snapshot);
        REQUIRE(fault->createdPaths.size() >= 2);
        REQUIRE(readStorageBytes(fault->collisionPath) == "foreign collision bytes");
        REQUIRE(readStorageBytes(fault->collisionTarget) == "foreign collision bytes");
        if (symlink) {
            REQUIRE(std::filesystem::is_symlink(fault->collisionPath));
        }
        for (const auto &path : fault->removedPaths) {
            REQUIRE(path != fault->collisionPath);
        }
        for (const auto &path : fault->createdPaths) {
            if (path != fault->collisionPath) {
                REQUIRE_FALSE(std::filesystem::exists(path));
            }
        }
    }
}

#if !defined(_WIN32)
TEST_CASE("a symlink replacing an inspected source cannot redirect a save",
          "[score][storage][conflict][symlink]") {
    StorageTemporaryDirectory directory;
    const auto source = storageV2Source();
    const std::string target = "unrelated target must be preserved";
    writeStorageBytes(directory.file(), source);
    writeStorageBytes(directory.file("target.json"), target);
    ScoreManager manager(directory.file());
    REQUIRE(manager.load());
    REQUIRE(manager.recordCompletedGame("memory", 2048, "Ada", "date"));
    const StorageSnapshot snapshot(manager);
    REQUIRE(std::filesystem::remove(directory.file()));
    std::filesystem::create_symlink(directory.file("target.json"), directory.file());

    REQUIRE_FALSE(manager.save());
    REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::SourceChanged);
    snapshot.requireUnchanged(manager);
    REQUIRE(std::filesystem::is_symlink(directory.file()));
    REQUIRE(readStorageBytes(directory.file("target.json")) == target);
    REQUIRE_FALSE(std::filesystem::exists(directory.file("scores.json.bak")));
}

TEST_CASE("a symlink backup cannot be treated as a writable or reusable migration backup",
          "[score][storage][migration][symlink]") {
    StorageTemporaryDirectory directory;
    const auto source = storageLegacySource();
    writeStorageBytes(directory.file(), source);
    writeStorageBytes(directory.file("backup-target.json"), source);
    std::filesystem::create_symlink(directory.file("backup-target.json"),
                                    directory.file("scores.json.bak"));
    ScoreManager manager(directory.file());
    REQUIRE(manager.load());
    const StorageSnapshot snapshot(manager);
    REQUIRE_FALSE(manager.save());
    REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::BackupConflict);
    snapshot.requireUnchanged(manager);
    REQUIRE(readStorageBytes(directory.file()) == source);
    REQUIRE(std::filesystem::is_symlink(directory.file("scores.json.bak")));
    REQUIRE(readStorageBytes(directory.file("backup-target.json")) == source);
}

TEST_CASE("a dangling symlink cannot be overwritten as an expected missing source",
          "[score][storage][conflict][symlink]") {
    StorageTemporaryDirectory directory;
    ScoreManager manager(directory.file());
    REQUIRE(manager.recordCompletedGame("memory", 512, "Ada", "date"));
    const StorageSnapshot snapshot(manager);
    std::filesystem::create_symlink(directory.file("absent-target.json"), directory.file());
    REQUIRE_FALSE(manager.save());
    REQUIRE(manager.lastSaveStatus() == ScoreSaveStatus::SourceChanged);
    snapshot.requireUnchanged(manager);
    REQUIRE(std::filesystem::is_symlink(directory.file()));
    REQUIRE_FALSE(std::filesystem::exists(directory.file("absent-target.json")));
}
#endif
