#include "app/RuntimeCheck.hpp"

#include "app/AssetCatalog.hpp"
#include "app/AssetResolver.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace app {
namespace {

using Json = nlohmann::json;

std::string pathText(const std::filesystem::path &path) {
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
}

const char *sourceText(PathSource source) {
    switch (source) {
    case PathSource::Override:
        return "override";
    case PathSource::PlatformUserData:
        return "platform_user_data";
    case PathSource::BundleResources:
        return "bundle_resources";
    case PathSource::ExecutableDirectory:
        return "executable_directory";
    }
    return "unknown";
}

const char *pathErrorText(PathErrorCode code) {
    switch (code) {
    case PathErrorCode::MissingLocation:
        return "missing_location";
    case PathErrorCode::EmptyPath:
        return "empty_path";
    case PathErrorCode::RelativePath:
        return "relative_path";
    case PathErrorCode::EmbeddedNull:
        return "embedded_null";
    case PathErrorCode::NotDirectory:
        return "not_directory";
    case PathErrorCode::IoError:
        return "io_error";
    }
    return "unknown";
}

const char *assetErrorText(AssetErrorCode code) {
    switch (code) {
    case AssetErrorCode::EmptyPath:
        return "empty_path";
    case AssetErrorCode::AbsolutePath:
        return "absolute_path";
    case AssetErrorCode::EmbeddedNull:
        return "embedded_null";
    case AssetErrorCode::ParentTraversal:
        return "parent_traversal";
    case AssetErrorCode::Missing:
        return "missing";
    case AssetErrorCode::WrongType:
        return "wrong_type";
    case AssetErrorCode::IoError:
        return "io_error";
    }
    return "unknown";
}

void addError(Json &report, const char *domain, const char *code, const std::filesystem::path &path,
              const std::error_code &error = {}) {
    report["errors"].push_back({{"domain", domain},
                                {"code", code},
                                {"path", pathText(path)},
                                {"message", error ? error.message() : std::string(code)}});
}

void inspectResource(Json &report, const AssetPaths &paths, const char *relativePath) {
    const auto data = readAssetFile(paths, relativePath);
    report["resources"].push_back({{"relative_path", relativePath},
                                   {"path", pathText(paths.directory() / relativePath)},
                                   {"readable", !data.error.has_value()},
                                   {"bytes", data.bytes.size()}});
    if (data.error) {
        addError(report, "resource", assetErrorText(data.error->code), data.error->path,
                 data.error->systemError);
    } else if (data.bytes.empty()) {
        addError(report, "resource", "empty_file", paths.directory() / relativePath);
    }
}

} // namespace

RuntimeCheckResult inspectRuntime(const PathOptions &options,
                                  const AssetLocationProvider &assetProvider,
                                  const UserDataProvider &userDataProvider) {
    Json report{{"version", 1},
                {"success", false},
                {"assets", nullptr},
                {"user_data", nullptr},
                {"resources", Json::array()},
                {"errors", Json::array()},
                {"checks",
                 {{"user_profile_opened", false},
                  {"user_profile_written", false},
                  {"asset_validation", "binary_read"}}}};
    try {
        const auto assets = resolvePlatformAssetPaths(options, assetProvider);
        if (assets.error) {
            addError(report, "assets", pathErrorText(assets.error->code), assets.error->path,
                     assets.error->systemError);
        } else {
            const auto &paths = *assets.paths;
            report["assets"] = {{"directory", pathText(paths.directory())},
                                {"source", sourceText(paths.source())},
                                {"executable_dir", paths.executableDir()
                                                       ? Json(pathText(*paths.executableDir()))
                                                       : Json(nullptr)}};
            inspectResource(report, paths, kFontAsset);
            inspectResource(report, paths, kWindowIconAsset);
            for (const auto *relativePath : kSoundAssets) {
                inspectResource(report, paths, relativePath);
            }
        }
    } catch (const std::runtime_error &) {
        addError(report, "assets", "io_error", {}, std::make_error_code(std::errc::io_error));
    }
    try {
        const auto userData = resolvePlatformUserDataPaths(options, userDataProvider);
        if (userData.error) {
            addError(report, "user_data", pathErrorText(userData.error->code), userData.error->path,
                     userData.error->systemError);
        } else {
            const auto &paths = *userData.paths;
            report["user_data"] = {{"directory", pathText(paths.directory())},
                                   {"scores_file", pathText(paths.scoresFile())},
                                   {"settings_file", pathText(paths.settingsFile())},
                                   {"source", sourceText(paths.source())}};
        }
    } catch (const std::runtime_error &) {
        addError(report, "user_data", "io_error", {}, std::make_error_code(std::errc::io_error));
    }
    const bool success = report["errors"].empty();
    report["success"] = success;
    return {success, std::move(report)};
}

} // namespace app
