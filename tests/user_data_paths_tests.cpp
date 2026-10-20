#include "app/UserDataPaths.hpp"
#include "app/SettingsStore.hpp"
#include "core/ScoreManager.hpp"

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
using app::NativeUserDataResult;
using app::PathContext;
using app::PathErrorCode;
using app::PathKind;
using app::PathOptions;
using app::PathSource;
using app::UserDataResolution;

class TempTree {
  public:
    TempTree() {
        static std::atomic<unsigned> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 64; ++attempt) {
            root_ = fs::temp_directory_path() / ("numtilo_userdata_" + std::to_string(timestamp) +
                                                 "_" + std::to_string(sequence.fetch_add(1)));
            std::error_code error;
            if (fs::create_directory(root_, error)) {
                return;
            }
            if (error && error != std::errc::file_exists) {
                throw fs::filesystem_error("create temporary user data fixture", root_, error);
            }
        }
        throw std::runtime_error("could not claim a unique user data fixture");
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

fs::path embeddedNullPath(const TempTree &tree) {
    auto native = (tree.root() / "before-null").native();
    native.push_back(fs::path::value_type{});
    native += fs::path("after-null").native();
    return fs::path(native);
}

const app::UserDataPaths &requireSuccess(const UserDataResolution &result) {
    REQUIRE(result.paths.has_value());
    REQUIRE_FALSE(result.error.has_value());
    return *result.paths;
}

void requireFailure(const UserDataResolution &result, PathErrorCode code,
                    std::optional<fs::path> path = std::nullopt) {
    REQUIRE_FALSE(result.paths.has_value());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->kind == PathKind::UserData);
    REQUIRE(result.error->code == code);
    if (path) {
        REQUIRE(result.error->path == *path);
    }
}

void requireReady(const app::UserDataDirectoryResult &result) {
    REQUIRE(result.ready);
    REQUIRE_FALSE(result.error.has_value());
}

void requireBootstrapFailure(const app::UserDataDirectoryResult &result, const fs::path &path) {
    REQUIRE_FALSE(result.ready);
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->kind == PathKind::UserData);
    REQUIRE(result.error->path == path);
}

std::string readBytes(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    input.exceptions(std::ios::badbit);
    if (!input) {
        throw std::runtime_error("could not read user data fixture");
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

std::vector<fs::path> entryNames(const fs::path &root) {
    std::vector<fs::path> paths;
    for (const auto &entry : fs::recursive_directory_iterator(root)) {
        paths.push_back(entry.path().lexically_relative(root));
    }
    std::sort(paths.begin(), paths.end());
    return paths;
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

TEST_CASE("user data resolves independently from absent or invalid asset locations",
          "[user-data-paths]") {
    TempTree tree;
    PathContext context;
    context.userDataBase = tree.root() / "absent-platform-base";
    PathOptions options;

    SECTION("no asset inputs are needed") {
    }

    SECTION("invalid asset inputs are irrelevant") {
        context.executableDir = fs::path("relative-executable");
        context.bundleResourcesDir = fs::path{};
        options.assetOverride = embeddedNullPath(tree);
    }

    const auto before = snapshotTree(tree.root());
    const auto result = app::resolveUserDataPaths(context, options);
    const auto &paths = requireSuccess(result);
    REQUIRE(paths.directory() == *context.userDataBase / "numtilo");
    REQUIRE(paths.scoresFile() == paths.directory() / "scores.json");
    REQUIRE(paths.settingsFile() == paths.directory() / "settings.json");
    REQUIRE(paths.source() == PathSource::PlatformUserData);
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("an explicit profile override bypasses native discovery and unrelated assets",
          "[user-data-paths]") {
    TempTree tree;
    PathOptions options;
    options.userDataOverride = tree.root() / "selected-profile";
    options.assetOverride = fs::path("invalid-relative-assets");
    int providerCalls = 0;
    const app::UserDataProvider provider = [&] {
        ++providerCalls;
        return NativeUserDataResult{tree.root() / "must-not-use-native-base",
                                    std::make_error_code(std::errc::permission_denied)};
    };

    const auto before = snapshotTree(tree.root());
    const auto result = app::resolvePlatformUserDataPaths(options, provider);
    const auto &paths = requireSuccess(result);
    REQUIRE(providerCalls == 0);
    REQUIRE(paths.directory() == *options.userDataOverride);
    REQUIRE(paths.source() == PathSource::Override);
    REQUIRE(paths.scoresFile() == *options.userDataOverride / "scores.json");
    REQUIRE(paths.settingsFile() == *options.userDataOverride / "settings.json");
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("invalid profile overrides never call discovery or fall back", "[user-data-paths]") {
    TempTree tree;
    const auto fileRoot = tree.file("not-a-directory");
    const std::array<std::pair<fs::path, PathErrorCode>, 4> invalidInputs{{
        {fs::path{}, PathErrorCode::EmptyPath},
        {fs::path("relative-profile"), PathErrorCode::RelativePath},
        {embeddedNullPath(tree), PathErrorCode::EmbeddedNull},
        {fileRoot, PathErrorCode::NotDirectory},
    }};

    for (const auto &[input, code] : invalidInputs) {
        INFO("Expected error: " << static_cast<int>(code));
        PathOptions options;
        options.userDataOverride = input;
        int providerCalls = 0;
        const app::UserDataProvider provider = [&] {
            ++providerCalls;
            return NativeUserDataResult{tree.root() / "alternative-base", {}};
        };
        const auto before = snapshotTree(tree.root());
        const auto result = app::resolvePlatformUserDataPaths(options, provider);
        requireFailure(result, code, input);
        REQUIRE(providerCalls == 0);
        REQUIRE(snapshotTree(tree.root()) == before);
    }
}

TEST_CASE("native user data discovery is injected once and resolution stays read-only",
          "[user-data-paths]") {
    TempTree tree;
    const auto base = tree.root() / "missing" / "nested" / "platform-base";
    int providerCalls = 0;
    const app::UserDataProvider provider = [&] {
        ++providerCalls;
        return NativeUserDataResult{base, {}};
    };
    const auto before = snapshotTree(tree.root());

    const auto result = app::resolvePlatformUserDataPaths({}, provider);
    const auto &paths = requireSuccess(result);
    REQUIRE(providerCalls == 1);
    REQUIRE(paths.directory() == base / "numtilo");
    REQUIRE(paths.source() == PathSource::PlatformUserData);
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("missing or failed discovery returns only a structured user data error",
          "[user-data-paths]") {
    TempTree tree;
    const auto before = snapshotTree(tree.root());

    SECTION("no provider") {
        requireFailure(app::resolvePlatformUserDataPaths({}, app::UserDataProvider{}),
                       PathErrorCode::MissingLocation);
    }

    SECTION("provider has no location") {
        int providerCalls = 0;
        const app::UserDataProvider provider = [&] {
            ++providerCalls;
            return NativeUserDataResult{};
        };
        requireFailure(app::resolvePlatformUserDataPaths({}, provider),
                       PathErrorCode::MissingLocation);
        REQUIRE(providerCalls == 1);
    }

    SECTION("provider returns a native failure even alongside a location") {
        const auto base = tree.root() / "native-base";
        const auto nativeError = std::make_error_code(std::errc::permission_denied);
        int providerCalls = 0;
        const app::UserDataProvider provider = [&] {
            ++providerCalls;
            return NativeUserDataResult{base, nativeError};
        };
        const auto result = app::resolvePlatformUserDataPaths({}, provider);
        requireFailure(result, PathErrorCode::IoError, base);
        REQUIRE(result.error->systemError == nativeError);
        REQUIRE(providerCalls == 1);
    }

    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("invalid native base paths are rejected without writes or fallback",
          "[user-data-paths]") {
    TempTree tree;
    const std::array<std::pair<fs::path, PathErrorCode>, 3> invalidInputs{{
        {fs::path{}, PathErrorCode::EmptyPath},
        {fs::path("relative-native-base"), PathErrorCode::RelativePath},
        {embeddedNullPath(tree), PathErrorCode::EmbeddedNull},
    }};

    for (const auto &[input, code] : invalidInputs) {
        INFO("Expected error: " << static_cast<int>(code));
        int providerCalls = 0;
        const app::UserDataProvider provider = [&] {
            ++providerCalls;
            return NativeUserDataResult{input, {}};
        };
        const auto before = snapshotTree(tree.root());
        requireFailure(app::resolvePlatformUserDataPaths({}, provider), code, input);
        REQUIRE(providerCalls == 1);
        REQUIRE(snapshotTree(tree.root()) == before);
    }
}

TEST_CASE("bootstrap creates only the selected profile directory and required ancestors",
          "[user-data-paths][bootstrap]") {
    TempTree tree;
    const auto marker = tree.file("unrelated.txt", "preserve these bytes\n");
    const auto markerModified = fs::last_write_time(marker);
    PathOptions options;
    options.userDataOverride = tree.root() / "new" / "nested" / "profile";
    options.assetOverride = tree.root() / "must-not-create-assets";
    const auto resolution = app::resolvePlatformUserDataPaths(options, app::UserDataProvider{});
    const auto &paths = requireSuccess(resolution);
    REQUIRE_FALSE(fs::exists(paths.directory()));

    requireReady(app::ensureUserDataDirectory(paths));
    REQUIRE(fs::is_directory(paths.directory()));
    REQUIRE_FALSE(fs::exists(paths.scoresFile()));
    REQUIRE_FALSE(fs::exists(paths.settingsFile()));
    REQUIRE_FALSE(fs::exists(*options.assetOverride));
    REQUIRE(readBytes(marker) == "preserve these bytes\n");
    REQUIRE((fs::last_write_time(marker) == markerModified));
    std::vector<fs::path> expectedNames{"new", fs::path("new") / "nested",
                                        fs::path("new") / "nested" / "profile", "unrelated.txt"};
    std::sort(expectedNames.begin(), expectedNames.end());
    REQUIRE(entryNames(tree.root()) == expectedNames);
}

TEST_CASE("bootstrap works for a platform-selected profile without any assets",
          "[user-data-paths][bootstrap]") {
    TempTree tree;
    const auto base = tree.root() / "platform-base";
    const app::UserDataProvider provider = [base] { return NativeUserDataResult{base, {}}; };
    const auto resolution = app::resolvePlatformUserDataPaths({}, provider);
    const auto &paths = requireSuccess(resolution);

    requireReady(app::ensureUserDataDirectory(paths));
    REQUIRE(paths.directory() == base / "numtilo");
    REQUIRE(fs::is_directory(base / "numtilo"));
    REQUIRE_FALSE(fs::exists(paths.scoresFile()));
    REQUIRE_FALSE(fs::exists(paths.settingsFile()));
}

TEST_CASE("repeated bootstrap preserves resolved paths existing files and modification times",
          "[user-data-paths][bootstrap]") {
    TempTree tree;
    PathOptions options;
    options.userDataOverride = tree.directory("profile");
    const auto scores = tree.file("profile/scores.json", "existing score bytes\n");
    const auto settings = tree.file("profile/settings.json", "existing setting bytes\n");
    tree.file("profile/custom.dat", "other user data\n");
    const auto before = snapshotTree(tree.root());

    for (int iteration = 0; iteration < 8; ++iteration) {
        INFO("Bootstrap: " << iteration);
        const auto resolution = app::resolvePlatformUserDataPaths(options, app::UserDataProvider{});
        const auto &paths = requireSuccess(resolution);
        REQUIRE(paths.directory() == *options.userDataOverride);
        REQUIRE(paths.scoresFile() == scores);
        REQUIRE(paths.settingsFile() == settings);
        requireReady(app::ensureUserDataDirectory(paths));
        REQUIRE(snapshotTree(tree.root()) == before);
    }
}

TEST_CASE("bootstrap rechecks file obstructions introduced after read-only resolution",
          "[user-data-paths][bootstrap]") {
    TempTree tree;
    PathOptions options;

    SECTION("selected profile root changes into a regular file") {
        options.userDataOverride = tree.root() / "profile";
        const auto resolution = app::resolvePlatformUserDataPaths(options, app::UserDataProvider{});
        const auto &paths = requireSuccess(resolution);
        tree.file("profile", "keep this obstruction\n");
        const auto before = snapshotTree(tree.root());

        const auto result = app::ensureUserDataDirectory(paths);
        requireBootstrapFailure(result, paths.directory());
        REQUIRE(result.error->code == PathErrorCode::NotDirectory);
        REQUIRE(snapshotTree(tree.root()) == before);
    }

    SECTION("a missing ancestor changes into a regular file") {
        options.userDataOverride = tree.root() / "blocked" / "nested" / "profile";
        const auto resolution = app::resolvePlatformUserDataPaths(options, app::UserDataProvider{});
        const auto &paths = requireSuccess(resolution);
        tree.file("blocked", "keep this ancestor obstruction\n");
        const auto before = snapshotTree(tree.root());

        const auto result = app::ensureUserDataDirectory(paths);
        requireBootstrapFailure(result, paths.directory());
#ifndef _WIN32
        REQUIRE(result.error->code == PathErrorCode::IoError);
        REQUIRE(result.error->systemError == std::errc::not_a_directory);
#endif
        REQUIRE(snapshotTree(tree.root()) == before);
    }
    // Regular-file obstructions behave deterministically even when tests run as an administrator.
    // No chmod assumptions or process-global working-directory changes are required.
}

TEST_CASE("Unicode profile roots bootstrap with stable score and settings filenames",
          "[user-data-paths][bootstrap]") {
    TempTree tree;
    PathOptions options;
    options.userDataOverride = tree.root() / fs::path(u8"çalışma alanı/用户 profili");
    const auto resolution = app::resolvePlatformUserDataPaths(options, app::UserDataProvider{});
    const auto &paths = requireSuccess(resolution);

    requireReady(app::ensureUserDataDirectory(paths));
    REQUIRE(paths.directory() == *options.userDataOverride);
    REQUIRE(paths.scoresFile() == *options.userDataOverride / "scores.json");
    REQUIRE(paths.settingsFile() == *options.userDataOverride / "settings.json");
    REQUIRE(fs::is_directory(paths.directory()));
}

TEST_CASE("XDG discovery prefers an absolute data home without touching the filesystem",
          "[user-data-paths][xdg]") {
    TempTree tree;
    const auto xdg = tree.root() / "missing-xdg-base";
    const auto home = tree.root() / "missing-home";
    const auto before = snapshotTree(tree.root());
    const auto result = app::detail::selectXdgDataBase(xdg, home);

    REQUIRE_FALSE(static_cast<bool>(result.error));
    REQUIRE(result.base.has_value());
    REQUIRE(*result.base == xdg);
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("unset empty or relative XDG falls back to an absolute HOME data base",
          "[user-data-paths][xdg]") {
    TempTree tree;
    const auto home = tree.root() / "home";
    const std::array<std::optional<fs::path>, 3> xdgInputs{std::nullopt, fs::path{},
                                                           fs::path("relative-xdg")};
    const auto before = snapshotTree(tree.root());

    for (const auto &xdg : xdgInputs) {
        const auto selected = app::detail::selectXdgDataBase(xdg, home);
        REQUIRE_FALSE(static_cast<bool>(selected.error));
        REQUIRE(selected.base.has_value());
        REQUIRE(*selected.base == home / ".local" / "share");
        const auto resolution =
            app::resolvePlatformUserDataPaths({}, [selected] { return selected; });
        const auto &paths = requireSuccess(resolution);
        REQUIRE(paths.directory() == home / ".local" / "share" / "numtilo");
    }
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("missing or invalid HOME without absolute XDG yields no invented location",
          "[user-data-paths][xdg]") {
    TempTree tree;
    const std::array<std::optional<fs::path>, 3> invalidInputs{std::nullopt, fs::path{},
                                                               fs::path("relative-location")};
    const auto before = snapshotTree(tree.root());

    for (const auto &xdg : invalidInputs) {
        for (const auto &home : invalidInputs) {
            const auto selected = app::detail::selectXdgDataBase(xdg, home);
            REQUIRE_FALSE(selected.base.has_value());
            REQUIRE_FALSE(static_cast<bool>(selected.error));
            requireFailure(app::resolvePlatformUserDataPaths({}, [selected] { return selected; }),
                           PathErrorCode::MissingLocation);
        }
    }
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("actual native discovery reports locations as absolute paths",
          "[user-data-paths][native]") {
    // Query only: never resolve or bootstrap the real user's profile, and never read its contents.
    const auto result = app::discoverUserDataBase();
    if (!result.base) {
        SKIP("Native OS lookup reports no user data base in this environment");
    }
    if (result.base) {
        REQUIRE_FALSE(static_cast<bool>(result.error));
        REQUIRE_FALSE(result.base->empty());
        REQUIRE(result.base->is_absolute());
        REQUIRE(result.base->native().find(fs::path::value_type{}) == fs::path::string_type::npos);
    }
}

#ifndef _WIN32
TEST_CASE("bootstrap limits new profile permissions and preserves existing permissions",
          "[user-data-paths][bootstrap]") {
    TempTree tree;
    PathOptions options;
    options.userDataOverride = tree.root() / "profile";
    const auto resolution = app::resolvePlatformUserDataPaths(options, app::UserDataProvider{});
    const auto &paths = requireSuccess(resolution);
    requireReady(app::ensureUserDataDirectory(paths));
    const auto permissionMask = fs::perms::owner_all | fs::perms::group_all | fs::perms::others_all;
    REQUIRE((fs::status(paths.directory()).permissions() & permissionMask) == fs::perms::owner_all);

    const auto customPermissions =
        fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec;
    fs::permissions(paths.directory(), customPermissions, fs::perm_options::replace);
    requireReady(app::ensureUserDataDirectory(paths));
    REQUIRE((fs::status(paths.directory()).permissions() & permissionMask) == customPermissions);
}

TEST_CASE("bootstrap rejects dangling symlink obstructions introduced after resolution",
          "[user-data-paths][bootstrap][symlink]") {
    TempTree tree;
    PathOptions options;
    options.userDataOverride = tree.root() / "profile";
    const auto resolution = app::resolvePlatformUserDataPaths(options, app::UserDataProvider{});
    const auto &paths = requireSuccess(resolution);
    const auto missingTarget = tree.root() / "missing-target";
    directorySymlink(missingTarget, paths.directory());
    const auto before = entryNames(tree.root());

    const auto result = app::ensureUserDataDirectory(paths);
    requireBootstrapFailure(result, paths.directory());
    REQUIRE(fs::is_symlink(fs::symlink_status(paths.directory())));
    REQUIRE(fs::read_symlink(paths.directory()) == missingTarget);
    REQUIRE_FALSE(fs::exists(missingTarget));
    REQUIRE(entryNames(tree.root()) == before);
}

TEST_CASE("bootstrap does not collapse a parent traversal across a directory symlink",
          "[user-data-paths][bootstrap][symlink]") {
    TempTree tree;
    const auto target = tree.directory("actual/deep");
    const auto link = tree.root() / "link";
    directorySymlink(target, link);
    PathOptions options;
    options.userDataOverride = link / ".." / "profile";
    const auto resolution = app::resolvePlatformUserDataPaths(options, app::UserDataProvider{});
    const auto &paths = requireSuccess(resolution);

    requireReady(app::ensureUserDataDirectory(paths));
    REQUIRE(paths.directory() == *options.userDataOverride);
    REQUIRE(fs::is_directory(tree.root() / "actual" / "profile"));
    REQUIRE(fs::equivalent(paths.directory(), tree.root() / "actual" / "profile"));
    REQUIRE_FALSE(fs::exists(tree.root() / "profile"));
}
#endif

TEST_CASE("Numtilo profiles persist scores and settings without importing a legacy sibling profile",
          "[user-data-paths][settings][score][integration]") {
    TempTree tree;
    const auto base = tree.root() / fs::path(u8"ユーザー verileri");
    // This old product directory is deliberate historical data, not the selected default.
    const auto legacyProfile = base / "sfml-2048";
    const auto oldScores = tree.file(legacyProfile / "scores.json", "old scores must remain\n");
    const auto oldSettings =
        tree.file(legacyProfile / "settings.json", "old settings must remain\n");
    const auto legacyBefore = snapshotTree(legacyProfile);
    const app::UserDataProvider provider = [base] { return NativeUserDataResult{base, {}}; };
    const auto resolved = app::resolvePlatformUserDataPaths({}, provider);
    const auto &paths = requireSuccess(resolved);
    REQUIRE(paths.directory() == base / "numtilo");
    REQUIRE_FALSE(fs::exists(paths.directory()));
    requireReady(app::ensureUserDataDirectory(paths));

    app::SettingsStore settings(paths.settingsFile());
    numtilo_core::ScoreManager scores(paths.scoresFile());
    REQUIRE(settings.load());
    REQUIRE(scores.load());
    REQUIRE(settings.setSoundEnabled(false));
    REQUIRE(scores.updateBestScore(8192));
    REQUIRE(settings.save());
    REQUIRE(scores.save());
    const auto before = snapshotTree(tree.root());

    const auto reopenedPaths = app::resolvePlatformUserDataPaths({}, provider);
    const auto &reopened = requireSuccess(reopenedPaths);
    requireReady(app::ensureUserDataDirectory(reopened));
    app::SettingsStore reopenedSettings(reopened.settingsFile());
    numtilo_core::ScoreManager reopenedScores(reopened.scoresFile());
    REQUIRE(reopenedSettings.load());
    REQUIRE(reopenedScores.load());
    REQUIRE_FALSE(reopenedSettings.soundEnabled());
    REQUIRE(reopenedScores.bestScore() == 8192);
    REQUIRE(reopenedScores.completedGames().empty());
    REQUIRE(readBytes(oldScores) == "old scores must remain\n");
    REQUIRE(readBytes(oldSettings) == "old settings must remain\n");
    REQUIRE(snapshotTree(legacyProfile) == legacyBefore);
    REQUIRE(snapshotTree(tree.root()) == before);
}
