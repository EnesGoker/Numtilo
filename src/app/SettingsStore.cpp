#include "app/SettingsStore.hpp"

#include "app/detail/SettingsValidation.hpp"
#include "core/detail/FileSnapshot.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <system_error>
#include <utility>

namespace app {
namespace {

using Json = nlohmann::json;
using numtilo_core::FileSaveStatus;

bool isMissing(const std::filesystem::file_status &status, const std::error_code &error) {
    return status.type() == std::filesystem::file_type::not_found &&
           (!error || error == std::errc::no_such_file_or_directory);
}

} // namespace

SettingsStore::SettingsStore(std::filesystem::path settingsFilePath,
                             std::shared_ptr<numtilo_core::FileStorage> storage)
    : settingsFilePath_(std::move(settingsFilePath)),
      storage_(storage ? std::move(storage) : numtilo_core::makeFileStorage()) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(settingsFilePath_, error);
    writeAllowed_ = isMissing(status, error);
}

bool SettingsStore::load() {
    writeAllowed_ = false;
    lastLoadStatus_ = SettingsLoadStatus::IoError;

    auto snapshot = numtilo_core::detail::readFileSnapshot(settingsFilePath_);
    if (snapshot.status == numtilo_core::detail::FileReadStatus::Missing) {
        soundEnabled_ = true;
        dirty_ = false;
        migrationPending_ = false;
        sourceSnapshot_.reset();
        writeAllowed_ = true;
        lastLoadStatus_ = SettingsLoadStatus::Missing;
        return true;
    }
    if (snapshot.status != numtilo_core::detail::FileReadStatus::Read) {
        return false;
    }

    const auto parsed = detail::parseSettingsProfile(snapshot.bytes);
    lastLoadStatus_ = parsed.status;
    if (parsed.status != SettingsLoadStatus::LoadedV1 &&
        parsed.status != SettingsLoadStatus::LegacyMigrationPending) {
        return false;
    }
    // Commit memory and exact source bytes only after shared schema validation succeeds.
    soundEnabled_ = parsed.soundEnabled;
    sourceSnapshot_ = std::move(snapshot.bytes);
    migrationPending_ = parsed.status == SettingsLoadStatus::LegacyMigrationPending;
    dirty_ = migrationPending_;
    writeAllowed_ = true;
    return true;
}

bool SettingsStore::save() {
    if (!writeAllowed_) {
        lastSaveStatus_ = FileSaveStatus::WriteBlocked;
        return false;
    }
    if (!dirty_ && !migrationPending_) {
        lastSaveStatus_ = FileSaveStatus::NotSaved;
        return true;
    }

    std::string serialized;
    try {
        const Json root = {{"version", kFormatVersion}, {"sound_enabled", soundEnabled_}};
        serialized = root.dump(2) + '\n';
    } catch (const Json::exception &) {
        lastSaveStatus_ = FileSaveStatus::SerializationError;
        return false;
    }

    try {
        lastSaveStatus_ =
            storage_->save(settingsFilePath_, serialized, sourceSnapshot_, migrationPending_);
    } catch (const std::runtime_error &) {
        lastSaveStatus_ = FileSaveStatus::IoError;
    }
    if (lastSaveStatus_ != FileSaveStatus::Saved) {
        if (lastSaveStatus_ == FileSaveStatus::SourceChanged) {
            writeAllowed_ = false;
        }
        return false;
    }
    // Moving an already serialized snapshot avoids an allocation after commit.
    sourceSnapshot_ = std::move(serialized);
    dirty_ = false;
    migrationPending_ = false;
    return true;
}

bool SettingsStore::soundEnabled() const noexcept {
    return soundEnabled_;
}

bool SettingsStore::setSoundEnabled(const bool enabled) noexcept {
    if (soundEnabled_ == enabled) {
        return false;
    }
    soundEnabled_ = enabled;
    dirty_ = true;
    return true;
}

bool SettingsStore::isDirty() const noexcept {
    return dirty_;
}

bool SettingsStore::needsMigration() const noexcept {
    return migrationPending_;
}

SettingsLoadStatus SettingsStore::lastLoadStatus() const noexcept {
    return lastLoadStatus_;
}

FileSaveStatus SettingsStore::lastSaveStatus() const noexcept {
    return lastSaveStatus_;
}

const std::filesystem::path &SettingsStore::settingsFilePath() const noexcept {
    return settingsFilePath_;
}

} // namespace app
