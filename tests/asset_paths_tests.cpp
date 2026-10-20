#include "app/AssetLocations.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
using app::AssetPathResolution;
using app::NativeAssetLocations;
using app::PathContext;
using app::PathErrorCode;
using app::PathKind;
using app::PathOptions;
using app::PathSource;

class TempTree {
  public:
    TempTree() {
        static std::atomic<unsigned> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 64; ++attempt) {
            root_ =
                fs::temp_directory_path() / ("numtilo_asset_paths_" + std::to_string(timestamp) +
                                             "_" + std::to_string(sequence.fetch_add(1)));
            std::error_code error;
            if (fs::create_directory(root_, error)) {
                return;
            }
            if (error && error != std::errc::file_exists) {
                throw fs::filesystem_error("create temporary asset-path fixture", root_, error);
            }
        }
        throw std::runtime_error("could not claim a unique asset-path fixture");
    }

    ~TempTree() {
        std::error_code ignored;
        fs::remove_all(root_, ignored);
    }

    TempTree(const TempTree &) = delete;
    TempTree &operator=(const TempTree &) = delete;

    const fs::path &root() const noexcept {
        return root_;
    }

    fs::path directory(const fs::path &relative) const {
        const auto path = root_ / relative;
        fs::create_directories(path);
        return path;
    }

    fs::path file(const fs::path &relative, std::string_view contents = "fixture bytes") const {
        const auto path = root_ / relative;
        fs::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.close();
        return path;
    }

  private:
    fs::path root_;
};

const app::AssetPaths &requireSuccess(const AssetPathResolution &result) {
    REQUIRE(result.paths.has_value());
    REQUIRE_FALSE(result.error.has_value());
    return *result.paths;
}

void requireFailure(const AssetPathResolution &result, PathKind kind, PathErrorCode code,
                    std::optional<fs::path> path = std::nullopt) {
    REQUIRE_FALSE(result.paths.has_value());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->kind == kind);
    REQUIRE(result.error->code == code);
    if (path) {
        REQUIRE(result.error->path == *path);
    }
}

PathContext executableContext(const TempTree &tree) {
    PathContext context;
    context.executableDir = tree.directory("runtime");
    tree.directory("runtime/assets");
    return context;
}

fs::path embeddedNullPath(const TempTree &tree) {
    auto native = (tree.root() / "before-null").native();
    native.push_back(fs::path::value_type{});
    native += fs::path("after-null").native();
    return fs::path(native);
}

std::string readBytes(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    input.exceptions(std::ios::badbit);
    if (!input) {
        throw std::runtime_error("could not read asset-path fixture");
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct SnapshotEntry {
    fs::path path;
    fs::file_type type;
    fs::file_time_type modified;
    std::string bytes;

    bool operator==(const SnapshotEntry &) const = default;
};

std::vector<SnapshotEntry> snapshotTree(const fs::path &root) {
    std::vector<SnapshotEntry> snapshot;
    const auto capture = [&](const fs::path &path) {
        const auto status = fs::status(path);
        snapshot.push_back({path.lexically_relative(root), status.type(), fs::last_write_time(path),
                            fs::is_regular_file(status) ? readBytes(path) : std::string{}});
    };
    capture(root);
    for (const auto &entry : fs::recursive_directory_iterator(root)) {
        capture(entry.path());
    }
    std::sort(snapshot.begin(), snapshot.end(),
              [](const auto &left, const auto &right) { return left.path < right.path; });
    return snapshot;
}

#ifndef _WIN32
void directorySymlink(const fs::path &target, const fs::path &link) {
    std::error_code error;
    fs::create_directory_symlink(target, link, error);
    if (error) {
        SKIP("Directory symlinks unavailable: " << error.message());
    }
}
#endif

} // namespace

TEST_CASE("asset roots resolve without any user-data discovery or valid user-data inputs",
          "[asset-paths]") {
    TempTree tree;
    auto context = executableContext(tree);
    PathOptions options;

    SECTION("no user-data sources") {
    }

    SECTION("invalid user-data sources are irrelevant") {
        context.userDataBase = embeddedNullPath(tree);
        options.userDataOverride = fs::path("invalid-relative-profile");
    }

    const auto before = snapshotTree(tree.root());
    const auto result = app::resolveAssetPaths(context, options);
    const auto &paths = requireSuccess(result);
    REQUIRE(paths.directory() == *context.executableDir / "assets");
    REQUIRE(paths.source() == PathSource::ExecutableDirectory);
    REQUIRE(paths.executableDir() == context.executableDir);
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("an asset override names the final root and ignores unused layout inputs",
          "[asset-paths]") {
    TempTree tree;
    PathContext context;
    context.bundleResourcesDir = fs::path{};
    context.executableDir = embeddedNullPath(tree);
    context.userDataBase = fs::path("invalid-profile-base");
    PathOptions options;
    options.assetOverride = tree.directory("custom-final-root");
    options.userDataOverride = fs::path{};
    const auto result = app::resolveAssetPaths(context, options);
    const auto &paths = requireSuccess(result);

    REQUIRE(paths.directory() == *options.assetOverride);
    REQUIRE(paths.source() == PathSource::Override);
    REQUIRE_FALSE(paths.executableDir().has_value());
}

TEST_CASE("bundle resources select one complete root without exposing an unused executable",
          "[asset-paths]") {
    TempTree tree;
    auto context = executableContext(tree);
    context.bundleResourcesDir = tree.directory("Game.app/Contents/Resources");
    const auto selected = tree.directory("Game.app/Contents/Resources/assets");
    tree.file("runtime/assets/executable-only.txt");
    tree.file("Game.app/Contents/Resources/assets/bundle-only.txt");
    context.executableDir = fs::path("unused-relative-executable");
    const auto result = app::resolveAssetPaths(context);
    const auto &paths = requireSuccess(result);

    REQUIRE(paths.directory() == selected);
    REQUIRE(paths.source() == PathSource::BundleResources);
    REQUIRE_FALSE(paths.executableDir().has_value());
    REQUIRE(fs::exists(paths.directory() / "bundle-only.txt"));
    REQUIRE_FALSE(fs::exists(paths.directory() / "executable-only.txt"));
}

TEST_CASE("every selected asset layout rejects empty relative and native NUL inputs",
          "[asset-paths]") {
    TempTree tree;
    const std::array<std::pair<fs::path, PathErrorCode>, 3> inputs{{
        {fs::path{}, PathErrorCode::EmptyPath},
        {fs::path("relative-location"), PathErrorCode::RelativePath},
        {embeddedNullPath(tree), PathErrorCode::EmbeddedNull},
    }};
    for (int layout = 0; layout < 3; ++layout) {
        for (const auto &[input, code] : inputs) {
            INFO("Selected layout: " << layout << ", error: " << static_cast<int>(code));
            auto context = executableContext(tree);
            PathOptions options;
            PathKind kind = PathKind::Assets;
            if (layout == 0) {
                options.assetOverride = input;
            } else if (layout == 1) {
                context.bundleResourcesDir = input;
            } else {
                context.executableDir = input;
                kind = PathKind::ExecutableDirectory;
            }
            requireFailure(app::resolveAssetPaths(context, options), kind, code, input);
        }
    }
}

TEST_CASE("an invalid selected asset root never falls back to a lower priority root",
          "[asset-paths]") {
    TempTree tree;
    auto context = executableContext(tree);
    context.bundleResourcesDir = tree.directory("bundle-resources");
    tree.directory("bundle-resources/assets");
    PathOptions options;

    SECTION("missing override with both lower priority roots valid") {
        options.assetOverride = tree.root() / "missing-override";
        requireFailure(app::resolveAssetPaths(context, options), PathKind::Assets,
                       PathErrorCode::MissingLocation, *options.assetOverride);
    }

    SECTION("file override with both lower priority roots valid") {
        options.assetOverride = tree.file("file-override");
        requireFailure(app::resolveAssetPaths(context, options), PathKind::Assets,
                       PathErrorCode::NotDirectory, *options.assetOverride);
    }

    SECTION("missing bundle assets with valid executable assets") {
        context.bundleResourcesDir = tree.directory("other-resources");
        requireFailure(app::resolveAssetPaths(context), PathKind::Assets,
                       PathErrorCode::MissingLocation, *context.bundleResourcesDir / "assets");
    }

    SECTION("file bundle assets with valid executable assets") {
        context.bundleResourcesDir = tree.directory("file-resources");
        tree.file("file-resources/assets");
        requireFailure(app::resolveAssetPaths(context), PathKind::Assets,
                       PathErrorCode::NotDirectory, *context.bundleResourcesDir / "assets");
    }
}

TEST_CASE("selected executable layouts require their own existing directories", "[asset-paths]") {
    TempTree tree;
    PathContext context;
    tree.directory("assets");
    tree.directory("distribution/assets");

    SECTION("no native locations") {
        requireFailure(app::resolveAssetPaths(context), PathKind::Assets,
                       PathErrorCode::MissingLocation);
    }

    SECTION("missing executable directory") {
        context.executableDir = tree.root() / "missing-runtime";
        requireFailure(app::resolveAssetPaths(context), PathKind::ExecutableDirectory,
                       PathErrorCode::MissingLocation, *context.executableDir);
    }

    SECTION("file executable directory") {
        context.executableDir = tree.file("file-runtime");
        requireFailure(app::resolveAssetPaths(context), PathKind::ExecutableDirectory,
                       PathErrorCode::NotDirectory, *context.executableDir);
    }

    SECTION("assets in the executable parent are never used") {
        context.executableDir = tree.directory("distribution/bin");
        requireFailure(app::resolveAssetPaths(context), PathKind::Assets,
                       PathErrorCode::MissingLocation, *context.executableDir / "assets");
    }

    SECTION("asset root beside the executable is a file") {
        context.executableDir = tree.directory("file-assets-runtime");
        tree.file("file-assets-runtime/assets");
        requireFailure(app::resolveAssetPaths(context), PathKind::Assets,
                       PathErrorCode::NotDirectory, *context.executableDir / "assets");
    }
}

TEST_CASE(
    "file ancestors in selected asset roots report errors instead of using valid native assets",
    "[asset-paths]") {
    TempTree tree;
    auto context = executableContext(tree);
    const auto blocker = tree.file("file-parent");
    PathOptions options;
    fs::path expectedPath;

    SECTION("override is nested below a file") {
        options.assetOverride = blocker / "nested" / "assets";
        expectedPath = *options.assetOverride;
    }

    SECTION("bundle resources point to a file ancestor") {
        context.bundleResourcesDir = blocker;
        expectedPath = blocker / "assets";
    }

    const auto before = snapshotTree(tree.root());
    const auto result = app::resolveAssetPaths(context, options);
    REQUIRE_FALSE(result.paths.has_value());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->kind == PathKind::Assets);
    REQUIRE(result.error->path == expectedPath);
#ifndef _WIN32
    REQUIRE(result.error->code == PathErrorCode::IoError);
    REQUIRE(result.error->systemError == std::errc::not_a_directory);
#else
    REQUIRE((result.error->code == PathErrorCode::IoError ||
             result.error->code == PathErrorCode::MissingLocation));
#endif
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("asset-only and composite resolution agree for every layout", "[asset-paths]") {
    TempTree tree;
    auto context = executableContext(tree);
    context.userDataBase = tree.root() / "profile-base";
    PathOptions options;
    PathSource expectedSource = PathSource::ExecutableDirectory;
    fs::path expectedRoot = *context.executableDir / "assets";

    SECTION("executable layout") {
    }

    SECTION("bundle layout") {
        context.bundleResourcesDir = tree.directory("resources");
        expectedRoot = tree.directory("resources/assets");
        expectedSource = PathSource::BundleResources;
    }

    SECTION("override layout") {
        options.assetOverride = tree.directory("override-root");
        expectedRoot = *options.assetOverride;
        expectedSource = PathSource::Override;
    }

    const auto pure = app::resolveAssetPaths(context, options);
    const auto &assets = requireSuccess(pure);
    const auto composite = app::resolveAppPaths(context, options);
    REQUIRE(composite.paths.has_value());
    REQUIRE_FALSE(composite.error.has_value());
    REQUIRE(assets.directory() == expectedRoot);
    REQUIRE(assets.source() == expectedSource);
    REQUIRE(composite.paths->assetDir() == assets.directory());
    REQUIRE(composite.paths->assetSource() == assets.source());
    REQUIRE(composite.paths->executableDir() == assets.executableDir());
    REQUIRE(composite.paths->userDataDir() == *context.userDataBase / "numtilo");
}

TEST_CASE("Unicode and spaces are preserved in selected asset roots", "[asset-paths]") {
    TempTree tree;
    PathContext context;
    context.bundleResourcesDir =
        tree.directory(fs::path(u8"çalışma alanı/游戏.app/Contents/Resources"));
    const auto expected =
        tree.directory(fs::path(u8"çalışma alanı/游戏.app/Contents/Resources/assets"));
    const auto result = app::resolveAssetPaths(context);
    const auto &paths = requireSuccess(result);
    REQUIRE(paths.directory() == expected);
    REQUIRE(paths.source() == PathSource::BundleResources);
    REQUIRE_FALSE(paths.executableDir().has_value());
}

TEST_CASE("repeated asset-root resolution modifies no assets profile files or timestamps",
          "[asset-paths]") {
    TempTree tree;
    const auto context = executableContext(tree);
    tree.file("runtime/assets/fonts/font.ttf", "keep font bytes\r\n");
    tree.file("separate-profile/scores.json", "keep score bytes\r\n");
    tree.file("separate-profile/settings.json", "keep setting bytes\r\n");
    const auto before = snapshotTree(tree.root());
    for (int iteration = 0; iteration < 8; ++iteration) {
        INFO("Resolution: " << iteration);
        const auto result = app::resolveAssetPaths(context);
        const auto &paths = requireSuccess(result);
        REQUIRE(paths.directory() == *context.executableDir / "assets");
        REQUIRE(snapshotTree(tree.root()) == before);
    }
}

TEST_CASE("native asset resolution with an override never calls discovery",
          "[asset-paths][native]") {
    TempTree tree;
    PathOptions options;
    options.assetOverride = tree.directory("explicit-assets");
    options.userDataOverride = embeddedNullPath(tree);
    int providerCalls = 0;
    const app::AssetLocationProvider provider = [&] {
        ++providerCalls;
        return NativeAssetLocations{std::nullopt, std::nullopt,
                                    std::make_error_code(std::errc::permission_denied)};
    };
    const auto result = app::resolvePlatformAssetPaths(options, provider);
    const auto &paths = requireSuccess(result);
    REQUIRE(providerCalls == 0);
    REQUIRE(paths.directory() == *options.assetOverride);
    REQUIRE(paths.source() == PathSource::Override);
    REQUIRE_FALSE(paths.executableDir().has_value());
}

TEST_CASE("invalid native asset overrides fail without consulting another location",
          "[asset-paths][native]") {
    TempTree tree;
    const auto validExecutable = tree.directory("runtime");
    tree.directory("runtime/assets");
    const std::array<std::pair<fs::path, PathErrorCode>, 5> inputs{{
        {fs::path{}, PathErrorCode::EmptyPath},
        {fs::path("relative-override"), PathErrorCode::RelativePath},
        {embeddedNullPath(tree), PathErrorCode::EmbeddedNull},
        {tree.root() / "absent-override", PathErrorCode::MissingLocation},
        {tree.file("file-override"), PathErrorCode::NotDirectory},
    }};
    const auto before = snapshotTree(tree.root());
    for (const auto &[input, code] : inputs) {
        INFO("Override error: " << static_cast<int>(code));
        PathOptions options;
        options.assetOverride = input;
        int providerCalls = 0;
        const app::AssetLocationProvider provider = [&] {
            ++providerCalls;
            return NativeAssetLocations{validExecutable, std::nullopt, {}};
        };
        requireFailure(app::resolvePlatformAssetPaths(options, provider), PathKind::Assets, code,
                       input);
        REQUIRE(providerCalls == 0);
    }
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("injected native asset locations preserve priority without profile lookup",
          "[asset-paths][native]") {
    TempTree tree;
    const auto executable = tree.directory("runtime");
    const auto executableAssets = tree.directory("runtime/assets");
    const auto resources = tree.directory("bundle-resources");
    const auto bundleAssets = tree.directory("bundle-resources/assets");
    NativeAssetLocations locations{executable, std::nullopt, {}};
    fs::path expected = executableAssets;
    PathSource source = PathSource::ExecutableDirectory;

    SECTION("executable native location") {
    }

    SECTION("bundle location with invalid unused executable") {
        locations.executableDir = fs::path{};
        locations.bundleResourcesDir = resources;
        expected = bundleAssets;
        source = PathSource::BundleResources;
    }

    int providerCalls = 0;
    const app::AssetLocationProvider provider = [&] {
        ++providerCalls;
        return locations;
    };
    const auto result = app::resolvePlatformAssetPaths({}, provider);
    const auto &paths = requireSuccess(result);
    REQUIRE(providerCalls == 1);
    REQUIRE(paths.directory() == expected);
    REQUIRE(paths.source() == source);
    REQUIRE(paths.executableDir().has_value() == (source == PathSource::ExecutableDirectory));
}

TEST_CASE("native asset discovery failures are typed and never trigger a fallback",
          "[asset-paths][native]") {
    TempTree tree;
    const auto executable = tree.directory("runtime");
    tree.directory("runtime/assets");
    const auto resources = tree.directory("bundle-resources");
    tree.directory("bundle-resources/assets");
    const auto before = snapshotTree(tree.root());

    SECTION("empty provider") {
        requireFailure(app::resolvePlatformAssetPaths({}, app::AssetLocationProvider{}),
                       PathKind::Assets, PathErrorCode::MissingLocation);
    }

    SECTION("provider reports no locations") {
        const app::AssetLocationProvider provider = [] { return NativeAssetLocations{}; };
        requireFailure(app::resolvePlatformAssetPaths({}, provider), PathKind::Assets,
                       PathErrorCode::MissingLocation);
    }

    SECTION("native error wins even when locations are supplied") {
        const auto nativeError = std::make_error_code(std::errc::permission_denied);
        const app::AssetLocationProvider provider = [=] {
            return NativeAssetLocations{executable, resources, nativeError};
        };
        const auto result = app::resolvePlatformAssetPaths({}, provider);
        requireFailure(result, PathKind::Assets, PathErrorCode::IoError, resources);
        REQUIRE(result.error->systemError == nativeError);
    }

    SECTION("system error thrown by provider retains its native reason") {
        const auto nativeError = std::make_error_code(std::errc::permission_denied);
        const app::AssetLocationProvider provider = [nativeError]() -> NativeAssetLocations {
            throw std::system_error(nativeError);
        };
        const auto result = app::resolvePlatformAssetPaths({}, provider);
        requireFailure(result, PathKind::Assets, PathErrorCode::IoError);
        REQUIRE(result.error->systemError == nativeError);
    }

    SECTION("runtime error thrown by provider becomes an IO error") {
        const app::AssetLocationProvider provider = []() -> NativeAssetLocations {
            throw std::runtime_error("injected native lookup failure");
        };
        const auto result = app::resolvePlatformAssetPaths({}, provider);
        requireFailure(result, PathKind::Assets, PathErrorCode::IoError);
        REQUIRE(static_cast<bool>(result.error->systemError));
    }

    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("invalid selected native locations are validated before looking for assets",
          "[asset-paths][native]") {
    TempTree tree;
    const auto executable = tree.directory("runtime");
    tree.directory("runtime/assets");
    const std::array<std::pair<fs::path, PathErrorCode>, 3> inputs{{
        {fs::path{}, PathErrorCode::EmptyPath},
        {fs::path("relative-native-location"), PathErrorCode::RelativePath},
        {embeddedNullPath(tree), PathErrorCode::EmbeddedNull},
    }};
    for (const auto &[input, code] : inputs) {
        INFO("Native location error: " << static_cast<int>(code));
        const app::AssetLocationProvider executableProvider = [input] {
            return NativeAssetLocations{input, std::nullopt, {}};
        };
        requireFailure(app::resolvePlatformAssetPaths({}, executableProvider),
                       PathKind::ExecutableDirectory, code, input);
        const app::AssetLocationProvider bundleProvider = [input, executable] {
            return NativeAssetLocations{executable, input, {}};
        };
        requireFailure(app::resolvePlatformAssetPaths({}, bundleProvider), PathKind::Assets, code,
                       input);
    }
}

TEST_CASE(
    "actual native asset discovery reports absolute native locations without profile bootstrap",
    "[asset-paths][native]") {
    // Read-only location query: no filesystem creation, asset lookup, or user-data query occurs.
    const auto result = app::discoverAssetLocations();
#if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
    REQUIRE_FALSE(static_cast<bool>(result.error));
#else
    if (result.error) {
        SKIP("Native OS reports an asset-location discovery error");
    }
#endif
    REQUIRE((result.executableDir.has_value() || result.bundleResourcesDir.has_value()));
    for (const auto &location : {result.executableDir, result.bundleResourcesDir}) {
        if (location) {
            REQUIRE_FALSE(location->empty());
            REQUIRE(location->is_absolute());
            REQUIRE(location->native().find(fs::path::value_type{}) == fs::path::string_type::npos);
        }
    }
}

#ifndef _WIN32
TEST_CASE("selected directory symlinks remain valid asset roots with their supplied spelling",
          "[asset-paths][symlink]") {
    TempTree tree;
    const auto target = tree.directory("actual-assets");
    const auto link = tree.root() / "assets-link";
    directorySymlink(target, link);
    PathOptions options;
    options.assetOverride = link;
    const auto result = app::resolveAssetPaths({}, options);
    const auto &paths = requireSuccess(result);
    REQUIRE(paths.directory() == link);
    REQUIRE(paths.source() == PathSource::Override);
}

TEST_CASE("asset roots preserve symlink parent traversal instead of lexical normalization",
          "[asset-paths][symlink]") {
    TempTree tree;
    const auto target = tree.directory("actual/deep");
    tree.directory("actual/assets");
    const auto link = tree.root() / "link";
    directorySymlink(target, link);
    PathOptions options;
    options.assetOverride = link / ".." / "assets";
    REQUIRE_FALSE(fs::exists(tree.root() / "assets"));
    const auto result = app::resolveAssetPaths({}, options);
    const auto &paths = requireSuccess(result);
    REQUIRE(paths.directory() == *options.assetOverride);
    REQUIRE(fs::equivalent(paths.directory(), tree.root() / "actual" / "assets"));
}

TEST_CASE("a dangling selected asset root never falls back to valid native roots",
          "[asset-paths][symlink]") {
    TempTree tree;
    const auto context = executableContext(tree);
    const auto target = tree.root() / "missing-target";
    const auto link = tree.root() / "dangling-assets";
    directorySymlink(target, link);
    PathOptions options;
    options.assetOverride = link;
    const auto result = app::resolveAssetPaths(context, options);
    REQUIRE_FALSE(result.paths.has_value());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->kind == PathKind::Assets);
    REQUIRE(result.error->path == link);
    REQUIRE(fs::read_symlink(link) == target);
    REQUIRE_FALSE(fs::exists(target));
}
#endif
