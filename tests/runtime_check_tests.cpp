#include "app/RuntimeCheck.hpp"

#include "app/AssetCatalog.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

constexpr std::array<const char *, 7> kExpectedResources{
    "fonts/Inter-Variable.ttf", "icons/numtilo.png",    "sounds/slide.wav",     "sounds/merge.wav",
    "sounds/spawn.wav",         "sounds/game_over.wav", "sounds/high_score.wav"};

class TempTree {
  public:
    TempTree() {
        static std::atomic<unsigned> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 64; ++attempt) {
            root_ =
                fs::temp_directory_path() / ("numtilo_runtime_check_" + std::to_string(timestamp) +
                                             "_" + std::to_string(sequence.fetch_add(1)));
            std::error_code error;
            if (fs::create_directory(root_, error)) {
                return;
            }
            if (error && error != std::errc::file_exists) {
                throw fs::filesystem_error("create runtime-check fixture", root_, error);
            }
        }
        throw std::runtime_error("could not claim a unique runtime-check fixture");
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
        const auto path = (root_ / relative).make_preferred();
        fs::create_directories(path);
        return path;
    }

    fs::path file(const fs::path &relative, std::string_view contents) const {
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

std::string pathText(const fs::path &path) {
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
}

std::string readBytes(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    input.exceptions(std::ios::badbit);
    if (!input) {
        throw std::runtime_error("could not read runtime-check fixture");
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
    std::vector<SnapshotEntry> entries;
    const auto capture = [&](const fs::path &path) {
        const auto status = fs::symlink_status(path);
        const bool link = fs::is_symlink(status);
        entries.push_back({path.lexically_relative(root), status.type(),
                           link ? fs::file_time_type{} : fs::last_write_time(path),
                           link                          ? pathText(fs::read_symlink(path))
                           : fs::is_regular_file(status) ? readBytes(path)
                                                         : std::string{}});
    };
    capture(root);
    for (const auto &entry : fs::recursive_directory_iterator(root)) {
        capture(entry.path());
    }
    std::sort(entries.begin(), entries.end(),
              [](const auto &left, const auto &right) { return left.path < right.path; });
    return entries;
}

fs::path resources(const TempTree &tree, const fs::path &relative = "resources") {
    const auto root = tree.directory(relative);
    for (std::size_t index = 0; index < kExpectedResources.size(); ++index) {
        // Runtime inspection reads bytes; it does not construct font, image or audio decoders.
        auto bytes = std::string("binary\r\n\0fixture", 16);
        bytes.append(index + 1, static_cast<char>('A' + index));
        tree.file(relative / kExpectedResources[index], bytes);
    }
    return root;
}

app::AssetLocationProvider executableProvider(const fs::path &directory) {
    return [directory] { return app::NativeAssetLocations{directory, std::nullopt, {}}; };
}

app::UserDataProvider userProvider(const fs::path &base) {
    return [base] { return app::NativeUserDataResult{base, {}}; };
}

app::PathOptions overrides(const fs::path &assets, const fs::path &profile) {
    app::PathOptions options;
    options.assetOverride = assets;
    options.userDataOverride = profile;
    return options;
}

void requireReport(const app::RuntimeCheckResult &result, bool success) {
    REQUIRE(result.success == success);
    REQUIRE(result.report.at("version") == 1);
    REQUIRE(result.report.at("success") == success);
    REQUIRE(result.report.at("resources").is_array());
    REQUIRE(result.report.at("errors").is_array());
    REQUIRE(result.report.at("checks").at("user_profile_opened") == false);
    REQUIRE(result.report.at("checks").at("user_profile_written") == false);
    REQUIRE(result.report.at("checks").at("asset_validation") == "binary_read");
    if (success) {
        REQUIRE(result.report.at("errors").empty());
    } else {
        REQUIRE_FALSE(result.report.at("errors").empty());
    }
    // The public diagnostic must remain independently serializable JSON.
    REQUIRE(Json::parse(result.report.dump()) == result.report);
}

const Json &resourceEntry(const Json &report, std::string_view relative) {
    const auto &entries = report.at("resources");
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto &entry) {
        return entry.at("relative_path") == Json(relative);
    });
    REQUIRE(found != entries.end());
    return *found;
}

void requireReadableResources(const Json &report, const fs::path &root) {
    REQUIRE(report.at("resources").size() == kExpectedResources.size());
    for (const auto *relative : kExpectedResources) {
        const auto &entry = resourceEntry(report, relative);
        REQUIRE(entry.at("path") == pathText(root / relative));
        REQUIRE(entry.at("readable") == true);
        REQUIRE(entry.at("bytes") == fs::file_size(root / relative));
    }
}

void requireError(const Json &report, std::string_view domain, std::string_view code,
                  const fs::path &path) {
    const auto &errors = report.at("errors");
    const auto found = std::find_if(errors.begin(), errors.end(), [&](const auto &error) {
        return error.at("domain") == Json(domain) && error.at("code") == Json(code) &&
               error.at("path") == pathText(path);
    });
    REQUIRE(found != errors.end());
    REQUIRE(found->at("message").is_string());
    REQUIRE_FALSE(found->at("message").get<std::string>().empty());
}

fs::path embeddedNullPath(const fs::path &prefix) {
    auto native = prefix.native();
    native.push_back(fs::path::value_type{});
    native += fs::path("suffix").native();
    return fs::path(native);
}

} // namespace

TEST_CASE("runtime inspection reports the shared asset catalog without bootstrapping a profile",
          "[runtime-check][window-icon]") {
    TempTree tree;
    const auto executable = tree.directory("application");
    const auto assets = resources(tree, "application/assets");
    const auto base = tree.root() / "absent" / "nested" / "native-data";
    const auto before = snapshotTree(tree.root());

    REQUIRE(std::string(app::kFontAsset) == kExpectedResources.front());
    REQUIRE(std::string(app::kWindowIconAsset) == kExpectedResources[1]);
    REQUIRE(app::kSoundAssets.size() == kExpectedResources.size() - 2);
    for (std::size_t index = 0; index < app::kSoundAssets.size(); ++index) {
        REQUIRE(std::string(app::kSoundAssets[index]) == kExpectedResources[index + 2]);
    }
    const auto result = app::inspectRuntime({}, executableProvider(executable), userProvider(base));

    requireReport(result, true);
    requireReadableResources(result.report, assets);
    REQUIRE(result.report.at("assets").at("directory") == pathText(assets));
    REQUIRE(result.report.at("assets").at("source") == "executable_directory");
    REQUIRE(result.report.at("assets").at("executable_dir") == pathText(executable));
    const auto profile = base / "numtilo";
    REQUIRE(result.report.at("user_data").at("directory") == pathText(profile));
    REQUIRE(result.report.at("user_data").at("source") == "platform_user_data");
    REQUIRE(result.report.at("user_data").at("scores_file") == pathText(profile / "scores.json"));
    REQUIRE(result.report.at("user_data").at("settings_file") ==
            pathText(profile / "settings.json"));
    REQUIRE_FALSE(fs::exists(base));
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("runtime inspection leaves existing unknown profile files and backups untouched",
          "[runtime-check]") {
    TempTree tree;
    const auto assets = resources(tree);
    const auto profile = tree.directory("profile");
    tree.file("profile/scores.json", "{\"version\":999,\"opaque\":true}\r\n");
    tree.file("profile/settings.json", std::string("bad\0settings", 12));
    tree.file("profile/scores.json.bak", "unrelated score backup");
    tree.file("profile/settings.json.bak", "unrelated settings backup");
    const auto before = snapshotTree(tree.root());

    const auto first = app::inspectRuntime(overrides(assets, profile), {}, {});
    const auto second = app::inspectRuntime(overrides(assets, profile), {}, {});

    requireReport(first, true);
    requireReport(second, true);
    REQUIRE(second.report == first.report);
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("Unicode overrides are reported as UTF-8 and bypass both native providers",
          "[runtime-check]") {
    TempTree tree;
    const auto assets = resources(tree, fs::path(u8"kaynaklar ü 中文"));
    const auto profile = tree.root() / fs::path(u8"oyuncu verisi İ 日本語") / "nested";
    int assetCalls = 0;
    int userCalls = 0;
    const auto before = snapshotTree(tree.root());

    const auto result = app::inspectRuntime(
        overrides(assets, profile),
        [&]() -> app::NativeAssetLocations {
            ++assetCalls;
            throw std::runtime_error("unused asset provider");
        },
        [&]() -> app::NativeUserDataResult {
            ++userCalls;
            throw std::runtime_error("unused user provider");
        });

    requireReport(result, true);
    requireReadableResources(result.report, assets);
    REQUIRE(assetCalls == 0);
    REQUIRE(userCalls == 0);
    REQUIRE(result.report.at("assets").at("source") == "override");
    REQUIRE(result.report.at("assets").at("executable_dir").is_null());
    REQUIRE(result.report.at("user_data").at("source") == "override");
    REQUIRE(result.report.at("user_data").at("directory") == pathText(profile));
    REQUIRE_FALSE(fs::exists(profile));
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("invalid selected overrides do not invoke native discovery or hide the other domain",
          "[runtime-check]") {
    TempTree tree;
    const auto assets = resources(tree);
    const auto profile = tree.root() / "profile";
    auto options = overrides(assets, profile);
    int assetCalls = 0;
    int userCalls = 0;
    const auto assetProvider = [&] {
        ++assetCalls;
        return app::NativeAssetLocations{tree.root(), std::nullopt, {}};
    };
    const auto dataProvider = [&] {
        ++userCalls;
        return app::NativeUserDataResult{tree.root(), {}};
    };
    const auto before = snapshotTree(tree.root());

    SECTION("relative user override still permits complete resource inspection") {
        options.userDataOverride = "relative-profile";
        const auto result = app::inspectRuntime(options, assetProvider, dataProvider);
        requireReport(result, false);
        requireReadableResources(result.report, assets);
        REQUIRE(result.report.at("user_data").is_null());
        requireError(result.report, "user_data", "relative_path", "relative-profile");
    }
    SECTION("empty asset override still reports the explicit user profile") {
        options.assetOverride = fs::path{};
        const auto result = app::inspectRuntime(options, assetProvider, dataProvider);
        requireReport(result, false);
        REQUIRE(result.report.at("assets").is_null());
        REQUIRE(result.report.at("resources").empty());
        REQUIRE(result.report.at("user_data").at("directory") == pathText(profile));
        requireError(result.report, "assets", "empty_path", {});
    }
    SECTION("embedded native NUL produces an exact typed path diagnostic") {
        options.assetOverride = embeddedNullPath(assets);
        const auto result = app::inspectRuntime(options, assetProvider, dataProvider);
        requireReport(result, false);
        REQUIRE(result.report.at("assets").is_null());
        REQUIRE(result.report.at("user_data").at("directory") == pathText(profile));
        requireError(result.report, "assets", "embedded_null", *options.assetOverride);
    }
    REQUIRE(assetCalls == 0);
    REQUIRE(userCalls == 0);
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("runtime inspection reports unavailable locations independently without inventing roots",
          "[runtime-check]") {
    TempTree tree;
    const auto before = snapshotTree(tree.root());

    const auto result = app::inspectRuntime({}, {}, {});

    requireReport(result, false);
    REQUIRE(result.report.at("assets").is_null());
    REQUIRE(result.report.at("user_data").is_null());
    REQUIRE(result.report.at("resources").empty());
    REQUIRE(result.report.at("errors").size() == 2);
    requireError(result.report, "assets", "missing_location", {});
    requireError(result.report, "user_data", "missing_location", {});
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("a failed native domain preserves the successful domain and its selected path",
          "[runtime-check]") {
    TempTree tree;
    const auto assets = resources(tree, "application/assets");
    const auto base = tree.root() / "native-data";
    const auto failure = std::make_error_code(std::errc::permission_denied);
    const auto before = snapshotTree(tree.root());

    SECTION("asset discovery error does not suppress the user-data location") {
        const auto result = app::inspectRuntime(
            {}, [&] { return app::NativeAssetLocations{tree.root() / "application", {}, failure}; },
            userProvider(base));
        requireReport(result, false);
        REQUIRE(result.report.at("assets").is_null());
        REQUIRE(result.report.at("resources").empty());
        REQUIRE(result.report.at("user_data").at("directory") == pathText(base / "numtilo"));
        requireError(result.report, "assets", "io_error", tree.root() / "application");
    }
    SECTION("user-data discovery error does not suppress resource reads") {
        const auto result =
            app::inspectRuntime({}, executableProvider(tree.root() / "application"),
                                [&] { return app::NativeUserDataResult{base, failure}; });
        requireReport(result, false);
        requireReadableResources(result.report, assets);
        REQUIRE(result.report.at("user_data").is_null());
        requireError(result.report, "user_data", "io_error", base);
    }
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("provider exceptions become independent runtime diagnostics without filesystem writes",
          "[runtime-check]") {
    TempTree tree;
    const auto before = snapshotTree(tree.root());

    const auto result = app::inspectRuntime(
        {},
        []() -> app::NativeAssetLocations {
            throw std::system_error(std::make_error_code(std::errc::permission_denied),
                                    "asset discovery denied");
        },
        []() -> app::NativeUserDataResult {
            throw std::runtime_error("user discovery unavailable");
        });

    requireReport(result, false);
    REQUIRE(result.report.at("assets").is_null());
    REQUIRE(result.report.at("user_data").is_null());
    REQUIRE(result.report.at("errors").size() == 2);
    requireError(result.report, "assets", "io_error", {});
    requireError(result.report, "user_data", "io_error", {});
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("missing resources are diagnosed at the selected root without sibling fallback",
          "[runtime-check]") {
    TempTree tree;
    const auto assets = resources(tree, "selected");
    resources(tree, "assets");
    resources(tree, "application/assets");
    fs::remove(assets / "fonts/Inter-Variable.ttf");
    fs::remove(assets / "sounds/merge.wav");
    const auto profile = tree.root() / "profile";
    const auto before = snapshotTree(tree.root());

    const auto result = app::inspectRuntime(overrides(assets, profile), {}, {});

    requireReport(result, false);
    REQUIRE(result.report.at("resources").size() == kExpectedResources.size());
    REQUIRE(result.report.at("errors").size() == 2);
    REQUIRE(result.report.at("user_data").at("directory") == pathText(profile));
    for (const auto *relative : {"fonts/Inter-Variable.ttf", "sounds/merge.wav"}) {
        const auto &entry = resourceEntry(result.report, relative);
        REQUIRE(entry.at("readable") == false);
        REQUIRE(entry.at("bytes") == 0);
        REQUIRE(entry.at("path") == pathText(assets / relative));
        requireError(result.report, "resource", "missing", assets / relative);
    }
    for (const auto *relative : {"icons/numtilo.png", "sounds/slide.wav", "sounds/spawn.wav",
                                 "sounds/game_over.wav", "sounds/high_score.wav"}) {
        REQUIRE(resourceEntry(result.report, relative).at("readable") == true);
    }
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("an empty readable asset has a distinct runtime error without decoding resources",
          "[runtime-check]") {
    TempTree tree;
    const auto assets = resources(tree);
    tree.file("resources/fonts/Inter-Variable.ttf", "");
    const auto before = snapshotTree(tree.root());

    const auto result = app::inspectRuntime(overrides(assets, tree.root() / "profile"), {}, {});

    requireReport(result, false);
    const auto &font = resourceEntry(result.report, "fonts/Inter-Variable.ttf");
    REQUIRE(font.at("readable") == true);
    REQUIRE(font.at("bytes") == 0);
    REQUIRE(result.report.at("errors").size() == 1);
    requireError(result.report, "resource", "empty_file", assets / "fonts/Inter-Variable.ttf");
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("a directory at a required sound path is typed as a resource failure",
          "[runtime-check]") {
    TempTree tree;
    const auto assets = resources(tree);
    fs::remove(assets / "sounds/spawn.wav");
    fs::create_directory(assets / "sounds/spawn.wav");
    const auto before = snapshotTree(tree.root());

    const auto result = app::inspectRuntime(overrides(assets, tree.root() / "profile"), {}, {});

    requireReport(result, false);
    REQUIRE(result.report.at("resources").size() == kExpectedResources.size());
    REQUIRE(result.report.at("errors").size() == 1);
    const auto &sound = resourceEntry(result.report, "sounds/spawn.wav");
    REQUIRE(sound.at("readable") == false);
    REQUIRE(sound.at("bytes") == 0);
    requireError(result.report, "resource", "wrong_type", assets / "sounds/spawn.wav");
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("bundle runtime metadata selects its complete resources and ignores an unused executable",
          "[runtime-check]") {
    TempTree tree;
    const auto bundle = tree.directory("Game.app/Contents/Resources");
    const auto assets = resources(tree, "Game.app/Contents/Resources/assets");
    const auto profile = tree.root() / "profile";
    app::PathOptions options;
    options.userDataOverride = profile;
    const auto before = snapshotTree(tree.root());

    const auto result = app::inspectRuntime(
        options, [&] { return app::NativeAssetLocations{fs::path("unused-relative"), bundle, {}}; },
        {});

    requireReport(result, true);
    requireReadableResources(result.report, assets);
    REQUIRE(result.report.at("assets").at("directory") == pathText(assets));
    REQUIRE(result.report.at("assets").at("source") == "bundle_resources");
    REQUIRE(result.report.at("assets").at("executable_dir").is_null());
    REQUIRE(result.report.at("user_data").at("directory") == pathText(profile));
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("a missing window icon never falls back outside the selected asset root",
          "[runtime-check][window-icon]") {
    TempTree tree;
    const auto assets = resources(tree, "selected");
    resources(tree, "assets");
    resources(tree, "application/assets");
    fs::remove(assets / "icons/numtilo.png");
    const auto profile = tree.root() / "absent-profile";
    const auto before = snapshotTree(tree.root());

    const auto result = app::inspectRuntime(overrides(assets, profile), {}, {});

    requireReport(result, false);
    REQUIRE(result.report.at("resources").size() == kExpectedResources.size());
    REQUIRE(result.report.at("errors").size() == 1);
    REQUIRE(result.report.at("assets").at("directory") == pathText(assets));
    const auto &icon = resourceEntry(result.report, "icons/numtilo.png");
    REQUIRE(icon.at("path") == pathText(assets / "icons/numtilo.png"));
    REQUIRE(icon.at("readable") == false);
    REQUIRE(icon.at("bytes") == 0);
    requireError(result.report, "resource", "missing", assets / "icons/numtilo.png");
    for (const auto *relative : kExpectedResources) {
        if (std::string_view(relative) != "icons/numtilo.png") {
            REQUIRE(resourceEntry(result.report, relative).at("readable") == true);
        }
    }
    REQUIRE_FALSE(fs::exists(profile));
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("an empty window icon is readable but fails the mandatory resource contract",
          "[runtime-check][window-icon]") {
    TempTree tree;
    const auto assets = resources(tree);
    tree.file("resources/icons/numtilo.png", "");
    const auto profile = tree.root() / "absent-profile";
    const auto before = snapshotTree(tree.root());

    const auto result = app::inspectRuntime(overrides(assets, profile), {}, {});

    requireReport(result, false);
    REQUIRE(result.report.at("resources").size() == kExpectedResources.size());
    REQUIRE(result.report.at("errors").size() == 1);
    const auto &icon = resourceEntry(result.report, "icons/numtilo.png");
    REQUIRE(icon.at("readable") == true);
    REQUIRE(icon.at("bytes") == 0);
    requireError(result.report, "resource", "empty_file", assets / "icons/numtilo.png");
    REQUIRE_FALSE(fs::exists(profile));
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("nonempty window icon bytes are inspected without constructing an image decoder",
          "[runtime-check][window-icon]") {
    TempTree tree;
    const auto assets = resources(tree);
    const auto bytes = std::string("not a PNG\0binary icon", 21);
    tree.file("resources/icons/numtilo.png", bytes);
    const auto profile = tree.root() / "absent-profile";
    const auto before = snapshotTree(tree.root());

    const auto result = app::inspectRuntime(overrides(assets, profile), {}, {});

    requireReport(result, true);
    requireReadableResources(result.report, assets);
    REQUIRE(resourceEntry(result.report, "icons/numtilo.png").at("bytes") == bytes.size());
    REQUIRE_FALSE(fs::exists(profile));
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("a directory at the window icon path is a typed resource failure",
          "[runtime-check][window-icon]") {
    TempTree tree;
    const auto assets = resources(tree);
    const auto iconPath = assets / "icons/numtilo.png";
    fs::remove(iconPath);
    fs::create_directory(iconPath);
    const auto profile = tree.root() / "absent-profile";
    const auto before = snapshotTree(tree.root());

    const auto result = app::inspectRuntime(overrides(assets, profile), {}, {});

    requireReport(result, false);
    REQUIRE(result.report.at("resources").size() == kExpectedResources.size());
    REQUIRE(result.report.at("errors").size() == 1);
    REQUIRE(resourceEntry(result.report, "icons/numtilo.png").at("readable") == false);
    requireError(result.report, "resource", "wrong_type", iconPath);
    REQUIRE_FALSE(fs::exists(profile));
    REQUIRE((snapshotTree(tree.root()) == before));
}

#ifndef _WIN32
TEST_CASE("an unreadable window icon reports an I/O failure without touching the profile",
          "[runtime-check][window-icon][permissions]") {
    TempTree tree;
    const auto assets = resources(tree);
    const auto iconPath = assets / "icons/numtilo.png";
    const auto profile = tree.root() / "absent-profile";
    const auto before = snapshotTree(tree.root());
    struct PermissionRestore {
        fs::path path;
        fs::perms value;
        ~PermissionRestore() {
            std::error_code ignored;
            fs::permissions(path, value, fs::perm_options::replace, ignored);
        }
    } restore{iconPath, fs::status(iconPath).permissions()};
    std::error_code error;
    fs::permissions(iconPath, fs::perms::none, fs::perm_options::replace, error);
    if (error) {
        SKIP("Cannot restrict the isolated icon fixture: " << error.message());
    }
    {
        std::ifstream probe(iconPath, std::ios::binary);
        if (probe.is_open()) {
            SKIP("This process can read files despite the isolated permission restriction");
        }
    }

    const auto result = app::inspectRuntime(overrides(assets, profile), {}, {});
    fs::permissions(iconPath, restore.value, fs::perm_options::replace, error);
    REQUIRE_FALSE(error);

    requireReport(result, false);
    REQUIRE(result.report.at("resources").size() == kExpectedResources.size());
    REQUIRE(result.report.at("errors").size() == 1);
    const auto &icon = resourceEntry(result.report, "icons/numtilo.png");
    REQUIRE(icon.at("readable") == false);
    REQUIRE(icon.at("bytes") == 0);
    requireError(result.report, "resource", "io_error", iconPath);
    REQUIRE_FALSE(fs::exists(profile));
    REQUIRE((snapshotTree(tree.root()) == before));
}

TEST_CASE("window icon symlinks retain the existing asset resolver policy",
          "[runtime-check][window-icon][symlink]") {
    TempTree tree;
    const auto assets = resources(tree);
    const auto iconPath = assets / "icons/numtilo.png";
    const auto profile = tree.root() / "absent-profile";
    fs::path target;
    bool targetExists = false;
    SECTION("a readable target outside selected assets remains permitted") {
        target = tree.file("outside-selected/icon.png", "isolated external icon bytes");
        targetExists = true;
    }
    SECTION("a dangling target reports missing without a fallback") {
        target = tree.root() / "absent-outside/icon.png";
    }
    fs::remove(iconPath);
    std::error_code error;
    fs::create_symlink(target, iconPath, error);
    if (error) {
        SKIP("File symlinks unavailable in the isolated fixture: " << error.message());
    }
    const auto before = snapshotTree(tree.root());

    const auto result = app::inspectRuntime(overrides(assets, profile), {}, {});

    requireReport(result, targetExists);
    REQUIRE(result.report.at("resources").size() == kExpectedResources.size());
    const auto &icon = resourceEntry(result.report, "icons/numtilo.png");
    REQUIRE(icon.at("path") == pathText(iconPath));
    REQUIRE(icon.at("readable") == targetExists);
    if (targetExists) {
        REQUIRE(icon.at("bytes") == fs::file_size(target));
    } else {
        REQUIRE(result.report.at("errors").size() == 1);
        REQUIRE(icon.at("bytes") == 0);
        requireError(result.report, "resource", "missing", iconPath);
        REQUIRE_FALSE(fs::exists(target));
    }
    REQUIRE(fs::is_symlink(fs::symlink_status(iconPath)));
    REQUIRE(fs::read_symlink(iconPath) == target);
    REQUIRE_FALSE(fs::exists(profile));
    REQUIRE((snapshotTree(tree.root()) == before));
}
#endif
