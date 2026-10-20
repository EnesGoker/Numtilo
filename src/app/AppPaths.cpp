#include "app/AppPaths.hpp"

#include <utility>

namespace app {
namespace {

std::optional<PathError> validatePath(const std::filesystem::path &path, PathKind kind) {
    if (path.empty()) {
        return PathError{kind, PathErrorCode::EmptyPath, path, {}};
    }
    if (path.native().find(std::filesystem::path::value_type{}) !=
        std::filesystem::path::string_type::npos) {
        return PathError{kind, PathErrorCode::EmbeddedNull, path, {}};
    }
    if (!path.is_absolute()) {
        return PathError{kind, PathErrorCode::RelativePath, path, {}};
    }
    return std::nullopt;
}

std::optional<PathError> validateDirectory(const std::filesystem::path &path, PathKind kind,
                                           bool allowMissing) {
    if (auto error = validatePath(path, kind)) {
        return error;
    }
    // An absent user directory is allowed only below an existing directory. Inspect
    // ancestors without creating them: Windows can report a file/child path as
    // missing, and a dangling parent symlink must not look like a new data location.
    auto probePath = path;
    for (;;) {
        std::error_code error;
        auto status = std::filesystem::symlink_status(probePath, error);
        if (status.type() == std::filesystem::file_type::not_found &&
            (!error || error == std::errc::no_such_file_or_directory)) {
            const auto parent = probePath.parent_path();
            if (!allowMissing || parent.empty() || parent == probePath) {
                return PathError{kind, PathErrorCode::MissingLocation, path, error};
            }
            probePath = parent;
            continue;
        }
        if (error) {
            return PathError{kind, PathErrorCode::IoError, path, error};
        }
        if (std::filesystem::is_symlink(status)) {
            status = std::filesystem::status(probePath, error);
            if (error) {
                return PathError{kind, PathErrorCode::IoError, path, error};
            }
            if (status.type() == std::filesystem::file_type::not_found) {
                return PathError{kind, PathErrorCode::MissingLocation, path, {}};
            }
        }
        if (!std::filesystem::is_directory(status)) {
            return PathError{kind, PathErrorCode::NotDirectory, path, {}};
        }
        return std::nullopt;
    }
}

PathResolution failure(PathError error) {
    return {std::nullopt, std::move(error)};
}

} // namespace

UserDataPaths::UserDataPaths(std::filesystem::path directory, PathSource source)
    : directory_(std::move(directory)), source_(source) {
}

const std::filesystem::path &UserDataPaths::directory() const noexcept {
    return directory_;
}

std::filesystem::path UserDataPaths::scoresFile() const {
    return directory_ / "scores.json";
}

std::filesystem::path UserDataPaths::settingsFile() const {
    return directory_ / "settings.json";
}

PathSource UserDataPaths::source() const noexcept {
    return source_;
}

AssetPaths::AssetPaths(std::filesystem::path directory,
                       std::optional<std::filesystem::path> executableDir, PathSource source)
    : directory_(std::move(directory)), executableDir_(std::move(executableDir)), source_(source) {
}

const std::filesystem::path &AssetPaths::directory() const noexcept {
    return directory_;
}

const std::optional<std::filesystem::path> &AssetPaths::executableDir() const noexcept {
    return executableDir_;
}

PathSource AssetPaths::source() const noexcept {
    return source_;
}

AppPaths::AppPaths(UserDataPaths userData, AssetPaths assets)
    : userData_(std::move(userData)), assets_(std::move(assets)) {
}

const std::filesystem::path &AppPaths::userDataDir() const noexcept {
    return userData_.directory();
}

const std::filesystem::path &AppPaths::assetDir() const noexcept {
    return assets_.directory();
}

const std::optional<std::filesystem::path> &AppPaths::executableDir() const noexcept {
    return assets_.executableDir();
}

std::filesystem::path AppPaths::scoresFile() const {
    return userData_.scoresFile();
}

std::filesystem::path AppPaths::settingsFile() const {
    return userData_.settingsFile();
}

PathSource AppPaths::userDataSource() const noexcept {
    return userData_.source();
}

PathSource AppPaths::assetSource() const noexcept {
    return assets_.source();
}

UserDataResolution resolveUserDataPaths(const PathContext &context, const PathOptions &options) {
    if (!options.userDataOverride && !context.userDataBase) {
        return {std::nullopt,
                PathError{PathKind::UserData, PathErrorCode::MissingLocation, {}, {}}};
    }
    const auto &userDataInput =
        options.userDataOverride ? *options.userDataOverride : *context.userDataBase;
    if (auto error = validatePath(userDataInput, PathKind::UserData)) {
        return {std::nullopt, std::move(*error)};
    }
    const auto userDataDir = options.userDataOverride ? userDataInput : userDataInput / "numtilo";
    if (auto error = validateDirectory(userDataDir, PathKind::UserData, true)) {
        return {std::nullopt, std::move(*error)};
    }
    return {UserDataPaths(userDataDir, options.userDataOverride ? PathSource::Override
                                                                : PathSource::PlatformUserData),
            std::nullopt};
}

AssetPathResolution resolveAssetPaths(const PathContext &context, const PathOptions &options) {
    std::filesystem::path assetDir;
    std::optional<std::filesystem::path> executableDir;
    PathSource assetSource;
    if (options.assetOverride) {
        assetDir = *options.assetOverride;
        assetSource = PathSource::Override;
    } else if (context.bundleResourcesDir) {
        if (auto error = validatePath(*context.bundleResourcesDir, PathKind::Assets)) {
            return {std::nullopt, std::move(*error)};
        }
        assetDir = *context.bundleResourcesDir / "assets";
        assetSource = PathSource::BundleResources;
    } else if (context.executableDir) {
        if (auto error =
                validateDirectory(*context.executableDir, PathKind::ExecutableDirectory, false)) {
            return {std::nullopt, std::move(*error)};
        }
        executableDir = context.executableDir;
        assetDir = *context.executableDir / "assets";
        assetSource = PathSource::ExecutableDirectory;
    } else {
        return {std::nullopt, PathError{PathKind::Assets, PathErrorCode::MissingLocation, {}, {}}};
    }
    if (auto error = validateDirectory(assetDir, PathKind::Assets, false)) {
        return {std::nullopt, std::move(*error)};
    }

    return {AssetPaths(std::move(assetDir), std::move(executableDir), assetSource), std::nullopt};
}

PathResolution resolveAppPaths(const PathContext &context, const PathOptions &options) {
    auto userData = resolveUserDataPaths(context, options);
    if (userData.error) {
        return failure(std::move(*userData.error));
    }
    auto assets = resolveAssetPaths(context, options);
    if (assets.error) {
        return failure(std::move(*assets.error));
    }
    return {AppPaths(std::move(*userData.paths), std::move(*assets.paths)), std::nullopt};
}

} // namespace app
