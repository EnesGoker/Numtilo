#pragma once

#include "app/AssetLocations.hpp"
#include "app/UserDataPaths.hpp"

#include <nlohmann/json.hpp>

namespace app {

struct RuntimeCheckResult {
    bool success{false};
    nlohmann::json report;
};

// Read-only diagnostics: discover native roots and read the application's resource
// files. No profile file is opened, migrated, bootstrapped or saved; no SFML object
// or window is created. This verifies readable package contents, not decoding/UI.
RuntimeCheckResult
inspectRuntime(const PathOptions &options = {},
               const AssetLocationProvider &assetProvider = discoverAssetLocations,
               const UserDataProvider &userDataProvider = discoverUserDataBase);

} // namespace app
