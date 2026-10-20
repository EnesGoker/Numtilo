#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace numtilo_core {

enum class FileSaveStatus {
    NotSaved,
    Saved,
    WriteBlocked,
    SerializationError,
    SourceChanged,
    BackupConflict,
    IoError,
};

namespace detail {
class StorageFileOperations;
}

// Model-independent publication of a serialized user-data snapshot.
class FileStorage {
  public:
    virtual ~FileStorage() = default;

    // expectedSource is the exact last loaded/saved byte sequence; nullopt expects absence.
    virtual FileSaveStatus save(const std::filesystem::path &path, const std::string &serialized,
                                const std::optional<std::string> &expectedSource,
                                bool backupLegacy) = 0;
};

std::shared_ptr<FileStorage> makeFileStorage();
std::shared_ptr<FileStorage>
makeFileStorage(std::shared_ptr<detail::StorageFileOperations> operations);

} // namespace numtilo_core
