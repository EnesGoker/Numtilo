#pragma once

#include "app/AppPaths.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <system_error>

namespace app {

struct NativeAssetLocations {
    std::optional<std::filesystem::path> executableDir;
    std::optional<std::filesystem::path> bundleResourcesDir;
    std::error_code error;
};

// Read-only native discovery. No working-directory or environment asset search,
// directory creation, or profile-path discovery occurs here.
NativeAssetLocations discoverAssetLocations();
using AssetLocationProvider = std::function<NativeAssetLocations()>;

// A supplied asset override is authoritative and skips native discovery entirely.
AssetPathResolution
resolvePlatformAssetPaths(const PathOptions &options = {},
                          const AssetLocationProvider &provider = discoverAssetLocations);

} // namespace app
