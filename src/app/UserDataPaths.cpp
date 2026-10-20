#include "app/UserDataPaths.hpp"

#include <utility>

namespace app {

UserDataResolution resolvePlatformUserDataPaths(const PathOptions &options,
                                                const UserDataProvider &provider) {
    PathContext context;
    if (!options.userDataOverride) {
        if (!provider) {
            return {std::nullopt,
                    PathError{PathKind::UserData, PathErrorCode::MissingLocation, {}, {}}};
        }
        auto native = provider();
        if (native.error) {
            return {std::nullopt,
                    PathError{PathKind::UserData, PathErrorCode::IoError,
                              native.base.value_or(std::filesystem::path{}), native.error}};
        }
        context.userDataBase = std::move(native.base);
    }
    return resolveUserDataPaths(context, options);
}

UserDataDirectoryResult ensureUserDataDirectory(const UserDataPaths &paths) {
    PathOptions options;
    options.userDataOverride = paths.directory();
    // Revalidate after resolution; the filesystem may have changed in between.
    auto checked = resolveUserDataPaths({}, options);
    if (checked.error) {
        return {false, std::move(checked.error)};
    }
    std::error_code error;
    const bool created = std::filesystem::create_directories(paths.directory(), error);
    if (error) {
        return {false,
                PathError{PathKind::UserData, PathErrorCode::IoError, paths.directory(), error}};
    }
#ifndef _WIN32
    if (created) {
        std::filesystem::permissions(paths.directory(), std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace, error);
        if (error) {
            return {false, PathError{PathKind::UserData, PathErrorCode::IoError, paths.directory(),
                                     error}};
        }
    }
#else
    (void)created;
#endif
    checked = resolveUserDataPaths({}, options);
    if (checked.error) {
        return {false, std::move(checked.error)};
    }
    // Unlike read-only resolution, bootstrap must observe the actual directory.
    if (!std::filesystem::is_directory(paths.directory(), error)) {
        return {
            false,
            PathError{PathKind::UserData, PathErrorCode::IoError, paths.directory(),
                      error ? error : std::make_error_code(std::errc::no_such_file_or_directory)}};
    }
    return {true, std::nullopt};
}

namespace detail {

NativeUserDataResult selectXdgDataBase(const std::optional<std::filesystem::path> &xdgDataHome,
                                       const std::optional<std::filesystem::path> &home) {
    if (xdgDataHome && xdgDataHome->is_absolute() && !xdgDataHome->empty()) {
        return {*xdgDataHome, {}};
    }
    if (home && home->is_absolute() && !home->empty()) {
        return {*home / ".local" / "share", {}};
    }
    return {std::nullopt, {}};
}

} // namespace detail

} // namespace app
