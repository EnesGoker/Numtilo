#pragma once

#include "app/AppPaths.hpp"
#include "core/FileStorage.hpp"
#include "core/detail/FileSnapshot.hpp"

#include <functional>
#include <memory>

namespace app {

// Capture the pre-upgrade working directory once at startup, outside path resolution.
// Explicit data overrides never consult the CWD: use the supplied legacy source, or
// the selected profile itself when none is supplied. A legacy override requires a
// user-data override. Resolvers validate paths as absolute/nonempty/no-NUL later.
struct LegacyDataLocation {
    std::optional<std::filesystem::path> directory;
    std::error_code error;
};
LegacyDataLocation
captureLegacyDataLocation(const std::optional<std::filesystem::path> &userDataOverride = {},
                          const std::optional<std::filesystem::path> &legacyDataOverride = {});

enum class ProfileImportStatus {
    DestinationPresent,
    SourceMissing,
    Imported,
    InvalidLegacyLocation,
    DestinationIoError,
    SourceIoError,
    InvalidSource,
    UnsupportedVersion,
    SourceChanged,
    PublicationFailed,
};

struct ProfileImportResult {
    ProfileImportStatus status;
    std::filesystem::path source;
    std::filesystem::path destination;
    std::optional<numtilo_core::FileSaveStatus> saveStatus;
    std::error_code systemError;

    // An unresolved import protects this profile for the current process. Successful
    // profiles and confirmed source absence remain independent and usable.
    bool allowsProfileWrites() const noexcept;
};

struct ProfileMigrationResult {
    ProfileImportResult scores;
    ProfileImportResult settings;
};

// Injection seam for deterministic source changes/read failures; production uses
// the native reader, which rejects symlinks and non-regular files.
using LegacySnapshotReader =
    std::function<numtilo_core::detail::FileReadResult(const std::filesystem::path &)>;

// Import validated source bytes into confirmed-absent targets using atomic no-overwrite
// publication. Sources are never modified/deleted. Existing targets are authoritative,
// regardless of validity; score/settings format upgrades happen in their normal loaders.
// For native paths, try the resolved base's old sfml-2048 profile before legacy (CWD),
// independently per file. Only SourceMissing permits fallback; all other failures retain
// their write barrier. Explicit user-data overrides use only the supplied legacy source.
ProfileMigrationResult
migrateLegacyProfile(const UserDataPaths &paths, const LegacyDataLocation &legacy,
                     std::shared_ptr<numtilo_core::FileStorage> storage = nullptr,
                     const LegacySnapshotReader &reader = numtilo_core::detail::readFileSnapshot);

// Ordinary storage for successful/unneeded imports; a memory-only write barrier after
// an unresolved import. No mid-round reload/import retry can erase dirty memory.
std::shared_ptr<numtilo_core::FileStorage>
storageAfterProfileImport(const ProfileImportResult &result);

} // namespace app
