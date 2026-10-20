#include "app/AssetLocations.hpp"

#include <stdexcept>
#include <utility>

namespace app {

AssetPathResolution resolvePlatformAssetPaths(const PathOptions &options,
                                              const AssetLocationProvider &provider) {
    PathContext context;
    if (!options.assetOverride) {
        if (!provider) {
            return {std::nullopt,
                    PathError{PathKind::Assets, PathErrorCode::MissingLocation, {}, {}}};
        }
        NativeAssetLocations native;
        try {
            native = provider();
        } catch (const std::system_error &error) {
            return {std::nullopt,
                    PathError{PathKind::Assets, PathErrorCode::IoError, {}, error.code()}};
        } catch (const std::runtime_error &) {
            return {std::nullopt, PathError{PathKind::Assets,
                                            PathErrorCode::IoError,
                                            {},
                                            std::make_error_code(std::errc::io_error)}};
        }
        if (native.error) {
            const auto selectedPath = native.bundleResourcesDir
                                          ? *native.bundleResourcesDir
                                          : native.executableDir.value_or(std::filesystem::path{});
            return {std::nullopt, PathError{PathKind::Assets, PathErrorCode::IoError, selectedPath,
                                            native.error}};
        }
        context.executableDir = std::move(native.executableDir);
        context.bundleResourcesDir = std::move(native.bundleResourcesDir);
    }
    return resolveAssetPaths(context, options);
}

#if !defined(__APPLE__) && !defined(_WIN32) && !defined(__linux__)
NativeAssetLocations discoverAssetLocations() {
    return {std::nullopt, std::nullopt, std::make_error_code(std::errc::operation_not_supported)};
}
#endif

} // namespace app
