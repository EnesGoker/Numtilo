#include "app/AppPaths.hpp"

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
using app::PathContext;
using app::PathErrorCode;
using app::PathKind;
using app::PathOptions;
using app::PathResolution;
using app::PathSource;

// Every filesystem fixture belongs to one uniquely claimed temporary directory.
class TempTree {
  public:
    TempTree() {
        static std::atomic<unsigned> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 64; ++attempt) {
            root_ = fs::temp_directory_path() / ("numtilo_paths_" + std::to_string(timestamp) +
                                                 "_" + std::to_string(sequence.fetch_add(1)));
            std::error_code error;
            if (fs::create_directory(root_, error)) {
                return;
            }
            if (error && error != std::errc::file_exists) {
                throw fs::filesystem_error("create temporary path fixture", root_, error);
            }
        }
        throw std::runtime_error("could not claim a unique temporary path fixture");
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

    fs::path file(const fs::path &relative, std::string_view contents = "fixture") const {
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

const app::AppPaths &requireSuccess(const PathResolution &result) {
    REQUIRE(result.paths.has_value());
    REQUIRE_FALSE(result.error.has_value());
    return *result.paths;
}

void requireFailure(const PathResolution &result, PathKind kind,
                    std::optional<PathErrorCode> code = std::nullopt,
                    std::optional<fs::path> path = std::nullopt) {
    REQUIRE_FALSE(result.paths.has_value());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->kind == kind);
    if (code) {
        REQUIRE(result.error->code == *code);
    }
    if (path) {
        REQUIRE(result.error->path == *path);
    }
}

PathContext executableContext(const TempTree &tree) {
    PathContext context;
    context.userDataBase = tree.root() / "profile-base";
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

enum class InputDomain { UserOverride, UserBase, AssetOverride, BundleResources, Executable };

constexpr std::array<InputDomain, 5> kInputDomains{
    InputDomain::UserOverride, InputDomain::UserBase, InputDomain::AssetOverride,
    InputDomain::BundleResources, InputDomain::Executable};

constexpr std::string_view domainName(InputDomain domain) {
    switch (domain) {
    case InputDomain::UserOverride:
        return "user data override";
    case InputDomain::UserBase:
        return "platform user data base";
    case InputDomain::AssetOverride:
        return "asset override";
    case InputDomain::BundleResources:
        return "bundle resources";
    case InputDomain::Executable:
        return "executable directory";
    }
    return "unknown";
}

constexpr PathKind domainKind(InputDomain domain) {
    switch (domain) {
    case InputDomain::UserOverride:
    case InputDomain::UserBase:
        return PathKind::UserData;
    case InputDomain::AssetOverride:
    case InputDomain::BundleResources:
        return PathKind::Assets;
    case InputDomain::Executable:
        return PathKind::ExecutableDirectory;
    }
    return PathKind::Assets;
}

void setSelectedInput(InputDomain domain, const fs::path &input, PathContext &context,
                      PathOptions &options) {
    switch (domain) {
    case InputDomain::UserOverride:
        options.userDataOverride = input;
        break;
    case InputDomain::UserBase:
        context.userDataBase = input;
        break;
    case InputDomain::AssetOverride:
        options.assetOverride = input;
        break;
    case InputDomain::BundleResources:
        context.bundleResourcesDir = input;
        break;
    case InputDomain::Executable:
        context.executableDir = input;
        break;
    }
}

std::string readBytes(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    input.exceptions(std::ios::badbit);
    if (!input) {
        throw std::runtime_error("could not read path fixture");
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

TEST_CASE("application paths separate platform profile data from executable assets",
          "[app-paths]") {
    TempTree tree;
    const auto context = executableContext(tree);
    const auto result = app::resolveAppPaths(context);
    const auto &paths = requireSuccess(result);

    REQUIRE(paths.userDataDir() == tree.root() / "profile-base" / "numtilo");
    REQUIRE(paths.assetDir() == tree.root() / "runtime" / "assets");
    REQUIRE(paths.executableDir() == context.executableDir);
    REQUIRE(paths.userDataSource() == PathSource::PlatformUserData);
    REQUIRE(paths.assetSource() == PathSource::ExecutableDirectory);
    REQUIRE(paths.scoresFile() == paths.userDataDir() / "scores.json");
    REQUIRE(paths.settingsFile() == paths.userDataDir() / "settings.json");
    REQUIRE_FALSE(fs::exists(paths.userDataDir()));
}

TEST_CASE("each application path override is independent and names the final root", "[app-paths]") {
    TempTree tree;
    auto context = executableContext(tree);
    PathOptions options;

    SECTION("user data override does not require a valid platform base") {
        options.userDataOverride = tree.directory("custom-profile");
        context.userDataBase = fs::path{};
        const auto result = app::resolveAppPaths(context, options);
        const auto &paths = requireSuccess(result);

        REQUIRE(paths.userDataDir() == *options.userDataOverride);
        REQUIRE(paths.userDataSource() == PathSource::Override);
        REQUIRE(paths.assetDir() == *context.executableDir / "assets");
        REQUIRE(paths.assetSource() == PathSource::ExecutableDirectory);
        REQUIRE(paths.executableDir() == context.executableDir);
    }

    SECTION("asset override ignores unused bundle and executable inputs") {
        options.assetOverride = tree.directory("custom-assets");
        context.bundleResourcesDir = fs::path{};
        context.executableDir = fs::path("relative-unused-executable");
        const auto result = app::resolveAppPaths(context, options);
        const auto &paths = requireSuccess(result);

        REQUIRE(paths.userDataDir() == *context.userDataBase / "numtilo");
        REQUIRE(paths.userDataSource() == PathSource::PlatformUserData);
        REQUIRE(paths.assetDir() == *options.assetOverride);
        REQUIRE(paths.assetSource() == PathSource::Override);
        REQUIRE_FALSE(paths.executableDir().has_value());
    }

    SECTION("both overrides work without any discovery context") {
        context = {};
        options.userDataOverride = tree.root() / "absent-custom-profile";
        options.assetOverride = tree.directory("custom-assets");
        const auto result = app::resolveAppPaths(context, options);
        const auto &paths = requireSuccess(result);

        REQUIRE(paths.userDataDir() == *options.userDataOverride);
        REQUIRE(paths.assetDir() == *options.assetOverride);
        REQUIRE(paths.userDataSource() == PathSource::Override);
        REQUIRE(paths.assetSource() == PathSource::Override);
        REQUIRE_FALSE(paths.executableDir().has_value());
    }
}

TEST_CASE("bundle assets take priority as one complete root", "[app-paths]") {
    TempTree tree;
    auto context = executableContext(tree);
    context.bundleResourcesDir = tree.directory("bundle/Contents/Resources");
    const auto bundleAssets = tree.directory("bundle/Contents/Resources/assets");
    tree.file("runtime/assets/only-executable.txt");
    tree.file("bundle/Contents/Resources/assets/only-bundle.txt");

    // An unused executable input is deliberately invalid: no mixed layout is resolved.
    context.executableDir = embeddedNullPath(tree);
    const auto result = app::resolveAppPaths(context);
    const auto &paths = requireSuccess(result);
    REQUIRE(paths.assetDir() == bundleAssets);
    REQUIRE(paths.assetSource() == PathSource::BundleResources);
    REQUIRE_FALSE(paths.executableDir().has_value());
    REQUIRE_FALSE(fs::exists(paths.assetDir() / "only-executable.txt"));
    REQUIRE(fs::exists(paths.assetDir() / "only-bundle.txt"));
}

TEST_CASE("invalid selected roots never fall back to lower priority locations", "[app-paths]") {
    TempTree tree;
    auto context = executableContext(tree);
    context.bundleResourcesDir = tree.directory("bundle-resources");
    tree.directory("bundle-resources/assets");
    PathOptions options;

    SECTION("missing asset override does not use valid bundle or executable assets") {
        options.assetOverride = tree.root() / "missing-assets";
        requireFailure(app::resolveAppPaths(context, options), PathKind::Assets,
                       PathErrorCode::MissingLocation, *options.assetOverride);
    }

    SECTION("missing bundle assets do not use valid executable assets") {
        context.bundleResourcesDir = tree.directory("other-bundle-resources");
        requireFailure(app::resolveAppPaths(context), PathKind::Assets,
                       PathErrorCode::MissingLocation, *context.bundleResourcesDir / "assets");
    }

    SECTION("a file as user override does not use a valid profile base") {
        tree.directory("profile-base/numtilo");
        options.userDataOverride = tree.file("invalid-profile-root");
        requireFailure(app::resolveAppPaths(context, options), PathKind::UserData,
                       PathErrorCode::NotDirectory, *options.userDataOverride);
    }
}

TEST_CASE("path sources must be explicit and assets are never sought in executable parents",
          "[app-paths]") {
    TempTree tree;
    PathContext context;
    PathOptions options;

    SECTION("no sources gives a user data missing-location error") {
        requireFailure(app::resolveAppPaths(context), PathKind::UserData,
                       PathErrorCode::MissingLocation);
    }

    SECTION("an asset override cannot invent a user data source") {
        options.assetOverride = tree.directory("assets");
        requireFailure(app::resolveAppPaths(context, options), PathKind::UserData,
                       PathErrorCode::MissingLocation);
    }

    SECTION("a user data source cannot invent an asset source") {
        context.userDataBase = tree.root() / "profiles";
        tree.directory("assets");
        requireFailure(app::resolveAppPaths(context), PathKind::Assets,
                       PathErrorCode::MissingLocation);
    }

    SECTION("assets in the executable parent are not an alternate root") {
        context.userDataBase = tree.root() / "profiles";
        context.executableDir = tree.directory("distribution/bin");
        tree.directory("distribution/assets");
        requireFailure(app::resolveAppPaths(context), PathKind::Assets,
                       PathErrorCode::MissingLocation, *context.executableDir / "assets");
    }

    SECTION("a supplied missing executable directory is an error") {
        context.userDataBase = tree.root() / "profiles";
        context.executableDir = tree.root() / "missing-runtime";
        requireFailure(app::resolveAppPaths(context), PathKind::ExecutableDirectory,
                       PathErrorCode::MissingLocation, *context.executableDir);
    }
}

TEST_CASE("all selected input domains reject empty relative and native embedded NUL paths",
          "[app-paths]") {
    TempTree tree;
    const std::array<std::pair<fs::path, PathErrorCode>, 3> invalidInputs{{
        {fs::path{}, PathErrorCode::EmptyPath},
        {fs::path("relative-location"), PathErrorCode::RelativePath},
        {embeddedNullPath(tree), PathErrorCode::EmbeddedNull},
    }};

    for (const auto domain : kInputDomains) {
        for (const auto &[input, code] : invalidInputs) {
            INFO("Selected input: " << domainName(domain));
            INFO("Expected error code: " << static_cast<int>(code));
            auto context = executableContext(tree);
            PathOptions options;
            setSelectedInput(domain, input, context, options);
            requireFailure(app::resolveAppPaths(context, options), domainKind(domain), code, input);
        }
    }
}

TEST_CASE("selected asset profile and executable roots must be directories", "[app-paths]") {
    TempTree tree;
    auto context = executableContext(tree);
    PathOptions options;

    SECTION("asset override points to a file") {
        options.assetOverride = tree.file("asset-file");
        requireFailure(app::resolveAppPaths(context, options), PathKind::Assets,
                       PathErrorCode::NotDirectory, *options.assetOverride);
    }

    SECTION("platform profile root is a file") {
        tree.file("profile-base/numtilo");
        requireFailure(app::resolveAppPaths(context), PathKind::UserData,
                       PathErrorCode::NotDirectory, *context.userDataBase / "numtilo");
    }

    SECTION("bundle asset root is a file") {
        context.bundleResourcesDir = tree.directory("resources");
        tree.file("resources/assets");
        requireFailure(app::resolveAppPaths(context), PathKind::Assets, PathErrorCode::NotDirectory,
                       *context.bundleResourcesDir / "assets");
    }

    SECTION("executable directory is a file") {
        context.executableDir = tree.file("executable-file");
        requireFailure(app::resolveAppPaths(context), PathKind::ExecutableDirectory,
                       PathErrorCode::NotDirectory, *context.executableDir);
    }
}

TEST_CASE("a file ancestor cannot masquerade as a missing user directory", "[app-paths]") {
    TempTree tree;
    auto context = executableContext(tree);
    const auto blocker = tree.file("blocked-ancestor");
    PathOptions options;

    SECTION("user override is nested under a file") {
        options.userDataOverride = blocker / "nested" / "profile";
        const auto result = app::resolveAppPaths(context, options);
        requireFailure(result, PathKind::UserData, std::nullopt, *options.userDataOverride);
#ifndef _WIN32
        REQUIRE(result.error->code == PathErrorCode::IoError);
        REQUIRE(static_cast<bool>(result.error->systemError));
        REQUIRE(result.error->systemError == std::errc::not_a_directory);
#endif
    }

    SECTION("platform base is a file") {
        context.userDataBase = blocker;
        requireFailure(app::resolveAppPaths(context), PathKind::UserData, std::nullopt,
                       blocker / "numtilo");
    }

    SECTION("asset root is nested under a file") {
        options.assetOverride = blocker / "assets";
        requireFailure(app::resolveAppPaths(context, options), PathKind::Assets, std::nullopt,
                       *options.assetOverride);
    }
    // Native filesystem error mapping differs between POSIX and Win32. Failure and its selected
    // input are the contract; an invalid ancestor must never be accepted as an absent profile.
}

TEST_CASE("absent nested user data paths resolve without creating any filesystem entries",
          "[app-paths]") {
    TempTree tree;
    auto context = executableContext(tree);
    PathOptions options;

    SECTION("platform base may be absent with multiple missing ancestors") {
        context.userDataBase = tree.root() / "missing" / "nested" / "platform";
    }

    SECTION("user override may be absent with multiple missing ancestors") {
        options.userDataOverride = tree.root() / "missing" / "nested" / "profile";
    }

    const auto before = snapshotTree(tree.root());
    const auto result = app::resolveAppPaths(context, options);
    const auto &paths = requireSuccess(result);
    REQUIRE_FALSE(fs::exists(paths.userDataDir()));
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("repeated path resolution leaves existing data assets and timestamps untouched",
          "[app-paths]") {
    TempTree tree;
    const auto context = executableContext(tree);
    const auto scores = tree.file("profile-base/numtilo/scores.json", "existing score bytes\n");
    const auto settings =
        tree.file("profile-base/numtilo/settings.json", "existing settings bytes\n");
    tree.file("runtime/assets/font.ttf", "asset bytes\n");
    const auto before = snapshotTree(tree.root());

    for (int iteration = 0; iteration < 12; ++iteration) {
        INFO("Resolution: " << iteration);
        const auto result = app::resolveAppPaths(context);
        const auto &paths = requireSuccess(result);
        REQUIRE(paths.scoresFile() == scores);
        REQUIRE(paths.settingsFile() == settings);
        REQUIRE(snapshotTree(tree.root()) == before);
    }
}

TEST_CASE("Unicode and spaces remain intact in resolved roots and data filenames", "[app-paths]") {
    TempTree tree;
    PathContext context;
    context.userDataBase = tree.directory(fs::path(u8"çalışma dizini/用户 verileri"));
    context.bundleResourcesDir = tree.directory(fs::path(u8"Uygulama 名.app/Contents/Resources"));
    const auto assets = tree.directory(fs::path(u8"Uygulama 名.app/Contents/Resources/assets"));
    const auto result = app::resolveAppPaths(context);
    const auto &paths = requireSuccess(result);

    REQUIRE(paths.userDataDir() == *context.userDataBase / "numtilo");
    REQUIRE(paths.assetDir() == assets);
    REQUIRE(paths.scoresFile() == *context.userDataBase / "numtilo" / "scores.json");
    REQUIRE(paths.settingsFile() == *context.userDataBase / "numtilo" / "settings.json");
    REQUIRE_FALSE(paths.executableDir().has_value());
}

#ifndef _WIN32
TEST_CASE("directory symlink roots are accepted without replacing their supplied paths",
          "[app-paths][symlink]") {
    TempTree tree;
    const auto userTarget = tree.directory("actual-profile");
    const auto assetTarget = tree.directory("actual-assets");
    const auto userLink = tree.root() / "profile-link";
    const auto assetLink = tree.root() / "asset-link";
    directorySymlink(userTarget, userLink);
    directorySymlink(assetTarget, assetLink);
    PathOptions options;
    options.userDataOverride = userLink;
    options.assetOverride = assetLink;

    const auto result = app::resolveAppPaths({}, options);
    const auto &paths = requireSuccess(result);
    REQUIRE(paths.userDataDir() == userLink);
    REQUIRE(paths.assetDir() == assetLink);
    REQUIRE(paths.scoresFile() == userLink / "scores.json");
    REQUIRE(paths.settingsFile() == userLink / "settings.json");
    REQUIRE_FALSE(paths.executableDir().has_value());
}

TEST_CASE("symlink parent traversal retains native filesystem semantics", "[app-paths][symlink]") {
    TempTree tree;
    const auto target = tree.directory("actual/deep");
    tree.directory("actual/assets");
    const auto link = tree.root() / "link";
    directorySymlink(target, link);
    PathOptions options;
    options.userDataOverride = link / ".." / "profile";
    options.assetOverride = link / ".." / "assets";

    REQUIRE_FALSE(fs::exists(tree.root() / "assets"));
    const auto result = app::resolveAppPaths({}, options);
    const auto &paths = requireSuccess(result);
    REQUIRE(paths.userDataDir() == *options.userDataOverride);
    REQUIRE(paths.assetDir() == *options.assetOverride);
    REQUIRE(fs::equivalent(paths.assetDir(), tree.root() / "actual" / "assets"));
    REQUIRE_FALSE(fs::exists(tree.root() / "actual" / "profile"));
}

TEST_CASE("dangling symlinks are invalid selected roots and ancestors", "[app-paths][symlink]") {
    TempTree tree;
    const auto context = executableContext(tree);
    const auto dangling = tree.root() / "dangling";
    directorySymlink(tree.root() / "missing-target", dangling);
    PathOptions options;

    SECTION("dangling user root") {
        options.userDataOverride = dangling;
        requireFailure(app::resolveAppPaths(context, options), PathKind::UserData, std::nullopt,
                       *options.userDataOverride);
    }

    SECTION("dangling user ancestor with missing descendants") {
        options.userDataOverride = dangling / "nested" / "profile";
        requireFailure(app::resolveAppPaths(context, options), PathKind::UserData, std::nullopt,
                       *options.userDataOverride);
    }

    SECTION("dangling platform user data base") {
        auto danglingContext = context;
        danglingContext.userDataBase = dangling;
        requireFailure(app::resolveAppPaths(danglingContext), PathKind::UserData, std::nullopt,
                       dangling / "numtilo");
    }

    SECTION("dangling user root with trailing separator") {
        options.userDataOverride = dangling / "";
        requireFailure(app::resolveAppPaths(context, options), PathKind::UserData, std::nullopt,
                       *options.userDataOverride);
    }

    SECTION("dangling asset root") {
        options.assetOverride = dangling;
        requireFailure(app::resolveAppPaths(context, options), PathKind::Assets, std::nullopt,
                       *options.assetOverride);
    }

    SECTION("dangling asset root with trailing separator") {
        options.assetOverride = dangling / "";
        requireFailure(app::resolveAppPaths(context, options), PathKind::Assets, std::nullopt,
                       *options.assetOverride);
    }
}
#endif
