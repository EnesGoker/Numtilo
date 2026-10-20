#pragma once

#include <cstdint>
#include <optional>
#include <filesystem>

namespace app {

struct RunConfig {
    bool vSyncEnabled{true};
    std::optional<unsigned int> frameLimit;
    // Programmatic isolation for acceptance fixtures; no native lookup or ambient
    // CWD import when supplied. legacyDataOverride can select an isolated source.
    std::optional<std::filesystem::path> userDataOverride;
    // Final read-only assets directory; bypasses executable/bundle discovery.
    std::optional<std::filesystem::path> assetOverride;
    // Defaults to one startup CWD snapshot; an explicit legacy source requires
    // userDataOverride as well, so fixtures cannot touch a real native profile.
    std::optional<std::filesystem::path> legacyDataOverride;
};

int run(const RunConfig &config = {});

} // namespace app
