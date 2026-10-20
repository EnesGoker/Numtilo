#pragma once

#include "app/AppPaths.hpp"

#include <functional>

namespace app {

struct NativeUserDataResult {
    std::optional<std::filesystem::path> base;
    std::error_code error;
};

// Read-only OS lookup; never creates a folder. Windows/macOS use native APIs.
NativeUserDataResult discoverUserDataBase();
using UserDataProvider = std::function<NativeUserDataResult()>;

// An explicit user-data override bypasses native discovery entirely. Asset paths
// are irrelevant here, including assetOverride. Invalid inputs never use the CWD.
UserDataResolution
resolvePlatformUserDataPaths(const PathOptions &options = {},
                             const UserDataProvider &provider = discoverUserDataBase);

// Explicit bootstrap operation, separate from read-only resolution. Success means
// the directory exists, not that subsequent writes are guaranteed to succeed.
struct UserDataDirectoryResult {
    bool ready{false};
    std::optional<PathError> error;
};
UserDataDirectoryResult ensureUserDataDirectory(const UserDataPaths &paths);

namespace detail {
// Linux environment selection is testable without mutating process-global getenv.
// Relative/empty XDG is ignored. Without an absolute HOME no location is invented.
NativeUserDataResult selectXdgDataBase(const std::optional<std::filesystem::path> &xdgDataHome,
                                       const std::optional<std::filesystem::path> &home);
} // namespace detail

} // namespace app
