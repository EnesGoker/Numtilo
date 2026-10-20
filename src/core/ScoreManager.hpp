#pragma once

#include "core/ScoreStorage.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace numtilo_core {

struct ScoreEntry {
    int score{0};
    std::string playedAtUtc;
    std::string playerName;
    // Legacy migration assigns a stable identity from the original entry position.
    std::string gameId;
};

enum class ScoreLoadStatus {
    NotLoaded,
    Missing,
    LoadedV2,
    LegacyMigrationPending,
    InvalidData,
    UnsupportedVersion,
    IoError,
};

class ScoreManager {
  public:
    static constexpr std::size_t kTopScoreCount = 5;
    static constexpr int kFormatVersion = 2;

    explicit ScoreManager(std::filesystem::path scoreFilePath,
                          std::shared_ptr<ScoreStorage> storage = nullptr);

    bool load();
    bool save();
    ScoreLoadStatus lastLoadStatus() const noexcept;
    ScoreSaveStatus lastSaveStatus() const noexcept;
    bool needsMigration() const noexcept;

    bool updateBestScore(int score) noexcept;
    // The caller supplies the identity of a logically completed round.
    // Replaying an identity is a no-op, even if its payload differs.
    bool recordCompletedGame(std::string gameId, int score, std::string playerName = "Oyuncu",
                             std::optional<std::string> playedAtUtc = std::nullopt);

    const std::vector<ScoreEntry> &completedGames() const noexcept;
    std::vector<ScoreEntry> topScores() const;
    int bestScore() const noexcept;
    bool isDirty() const noexcept;
    const std::filesystem::path &scoreFilePath() const noexcept;

  private:
    static std::string currentUtcIso8601();

    std::filesystem::path scoreFilePath_;
    std::shared_ptr<ScoreStorage> storage_;
    // Binary bytes of the last successful load/save; nullopt means expected absent.
    std::optional<std::string> sourceSnapshot_;
    std::vector<ScoreEntry> completedGames_;
    int bestScore_{0};
    bool dirty_{false};
    ScoreLoadStatus lastLoadStatus_{ScoreLoadStatus::NotLoaded};
    ScoreSaveStatus lastSaveStatus_{ScoreSaveStatus::NotSaved};
    bool migrationPending_{false};
    // Unsupported/failed loads block writing independently of in-memory dirty.
    bool writeAllowed_{false};
};

} // namespace numtilo_core
