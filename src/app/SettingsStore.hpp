#pragma once

#include "core/FileStorage.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace app {

enum class SettingsLoadStatus {
    NotLoaded,
    Missing,
    LoadedV1,
    LegacyMigrationPending,
    InvalidData,
    UnsupportedVersion,
    IoError,
};

class SettingsStore {
  public:
    static constexpr int kFormatVersion = 1;

    explicit SettingsStore(std::filesystem::path settingsFilePath,
                           std::shared_ptr<numtilo_core::FileStorage> storage = nullptr);

    bool load();
    // A clean snapshot is a successful no-op, reported as FileSaveStatus::NotSaved.
    bool save();
    bool soundEnabled() const noexcept;
    bool setSoundEnabled(bool enabled) noexcept;
    bool isDirty() const noexcept;
    bool needsMigration() const noexcept;
    SettingsLoadStatus lastLoadStatus() const noexcept;
    numtilo_core::FileSaveStatus lastSaveStatus() const noexcept;
    const std::filesystem::path &settingsFilePath() const noexcept;

  private:
    std::filesystem::path settingsFilePath_;
    std::shared_ptr<numtilo_core::FileStorage> storage_;
    // Exact binary source bytes for safe replacement and an exact legacy backup.
    std::optional<std::string> sourceSnapshot_;
    bool soundEnabled_{true};
    bool dirty_{false};
    bool migrationPending_{false};
    bool writeAllowed_{false};
    SettingsLoadStatus lastLoadStatus_{SettingsLoadStatus::NotLoaded};
    numtilo_core::FileSaveStatus lastSaveStatus_{numtilo_core::FileSaveStatus::NotSaved};
};

} // namespace app
