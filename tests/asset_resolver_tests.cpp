#include "app/AssetResolver.hpp"

#include <catch2/catch_message.hpp>
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
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
using app::AssetErrorCode;
using app::AssetKind;

class TempTree {
  public:
    TempTree() {
        static std::atomic<unsigned> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 64; ++attempt) {
            root_ = fs::temp_directory_path() / ("numtilo_asset_read_" + std::to_string(timestamp) +
                                                 "_" + std::to_string(sequence.fetch_add(1)));
            std::error_code error;
            if (fs::create_directory(root_, error)) {
                return;
            }
            if (error && error != std::errc::file_exists) {
                throw fs::filesystem_error("create temporary asset fixture", root_, error);
            }
        }
        throw std::runtime_error("could not claim a unique asset fixture");
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

app::AssetPaths assetPaths(const TempTree &tree,
                           const fs::path &relative = fs::path("selected-assets")) {
    app::PathOptions options;
    options.assetOverride = tree.directory(relative);
    const auto result = app::resolveAssetPaths({}, options);
    REQUIRE(result.paths.has_value());
    REQUIRE_FALSE(result.error.has_value());
    return *result.paths;
}

void requireResolved(const app::AssetResolution &result, const fs::path &expected) {
    REQUIRE(result.resolvedPath.has_value());
    REQUIRE_FALSE(result.error.has_value());
    REQUIRE(*result.resolvedPath == expected);
}

void requireError(const app::AssetResolution &result, AssetErrorCode code, const fs::path &path) {
    REQUIRE_FALSE(result.resolvedPath.has_value());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->code == code);
    REQUIRE(result.error->path == path);
}

void requireReadError(const app::AssetReadResult &result, AssetErrorCode code,
                      const fs::path &path) {
    REQUIRE(result.bytes.empty());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->code == code);
    REQUIRE(result.error->path == path);
}

fs::path embeddedNullRelativePath() {
    auto native = fs::path("fonts/before-null").native();
    native.push_back(fs::path::value_type{});
    native += fs::path("after-null.ttf").native();
    return fs::path(native);
}

std::string readBytes(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    input.exceptions(std::ios::badbit);
    if (!input) {
        throw std::runtime_error("could not read asset fixture");
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
void assetSymlink(const fs::path &target, const fs::path &link) {
    std::error_code error;
    fs::create_symlink(target, link, error);
    if (error) {
        SKIP("Asset symlinks unavailable: " << error.message());
    }
}
#endif

} // namespace

TEST_CASE("resource names resolve beneath one selected asset root", "[asset-resolver]") {
    TempTree tree;
    const auto paths = assetPaths(tree);
    const auto font = tree.file("selected-assets/fonts/Inter-Regular.ttf", "font fixture bytes");
    const auto sounds = tree.directory("selected-assets/sounds");
    tree.file("selected-assets/sounds/merge.wav", "sound fixture bytes");

    requireResolved(app::resolveAssetPath(paths, fs::path("fonts") / "Inter-Regular.ttf"), font);
    requireResolved(app::resolveAssetPath(paths, "sounds", AssetKind::Directory), sounds);
    requireResolved(app::resolveAssetPath(paths, fs::path("fonts") / "." / "Inter-Regular.ttf"),
                    paths.directory() / "fonts" / "." / "Inter-Regular.ttf");
}

TEST_CASE("resource kind checks distinguish ordinary files from directories", "[asset-resolver]") {
    TempTree tree;
    const auto paths = assetPaths(tree);
    const auto font = tree.file("selected-assets/font.ttf", "fixture bytes");
    const auto sounds = tree.directory("selected-assets/sounds");

    requireError(app::resolveAssetPath(paths, "font.ttf", AssetKind::Directory),
                 AssetErrorCode::WrongType, font);
    requireError(app::resolveAssetPath(paths, "sounds", AssetKind::File), AssetErrorCode::WrongType,
                 sounds);
    requireReadError(app::readAssetFile(paths, "sounds"), AssetErrorCode::WrongType, sounds);
    requireResolved(app::resolveAssetPath(paths, ".", AssetKind::Directory),
                    paths.directory() / ".");
    requireError(app::resolveAssetPath(paths, "."), AssetErrorCode::WrongType,
                 paths.directory() / ".");
}

TEST_CASE("invalid resource names are rejected before lookup and produce no read payload",
          "[asset-resolver]") {
    TempTree tree;
    const auto paths = assetPaths(tree);
    const std::array<std::pair<fs::path, AssetErrorCode>, 7> inputs{{
        {fs::path{}, AssetErrorCode::EmptyPath},
        {tree.root() / "absolute.ttf", AssetErrorCode::AbsolutePath},
        {embeddedNullRelativePath(), AssetErrorCode::EmbeddedNull},
        {fs::path("..") / "font.ttf", AssetErrorCode::ParentTraversal},
        {fs::path("fonts") / ".." / "font.ttf", AssetErrorCode::ParentTraversal},
        {fs::path("fonts") / "..", AssetErrorCode::ParentTraversal},
        {fs::path("one") / ".." / "two" / ".." / "font.ttf", AssetErrorCode::ParentTraversal},
    }};
    const auto before = snapshotTree(tree.root());
    for (const auto &[input, code] : inputs) {
        INFO("Resource input error: " << static_cast<int>(code));
        requireError(app::resolveAssetPath(paths, input), code, input);
        requireError(app::resolveAssetPath(paths, input, AssetKind::Directory), code, input);
        requireReadError(app::readAssetFile(paths, input), code, input);
    }
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("missing selected assets never fall back to sibling roots or prefixed names",
          "[asset-resolver]") {
    TempTree tree;
    const auto paths = assetPaths(tree);
    tree.file("assets/fonts/Inter-Regular.ttf", "source-tree lookalike");
    tree.file("other-assets/fonts/Inter-Regular.ttf", "sibling root lookalike");
    tree.file("selected-assets/fonts/Existing.ttf", "existing selected asset");
    const auto fontName = fs::path("fonts") / "Inter-Regular.ttf";
    const auto before = snapshotTree(tree.root());

    requireError(app::resolveAssetPath(paths, fontName), AssetErrorCode::Missing,
                 paths.directory() / fontName);
    requireReadError(app::readAssetFile(paths, fontName), AssetErrorCode::Missing,
                     paths.directory() / fontName);
    const auto prefixedName = fs::path("assets") / "fonts" / "Existing.ttf";
    requireError(app::resolveAssetPath(paths, prefixedName), AssetErrorCode::Missing,
                 paths.directory() / prefixedName);
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("binary asset reads preserve NUL UTF-8 CRLF and chunk boundaries", "[asset-resolver]") {
    TempTree tree;
    const auto paths = assetPaths(tree);
    const std::u8string_view unicode = u8"İnci 🎮 用户";
    const std::string utf8{reinterpret_cast<const char *>(unicode.data()), unicode.size()};
    const std::array<std::size_t, 6> sizes{0, 1, 65535, 65536, 65537, 3 * 65536 + 17};
    for (const auto size : sizes) {
        INFO("Asset byte count: " << size);
        std::string expected(size, '\0');
        for (std::size_t index = 0; index < expected.size(); ++index) {
            expected[index] = static_cast<char>(index % 256);
        }
        if (size > utf8.size() + 2) {
            expected.replace(0, utf8.size(), utf8);
            expected[utf8.size()] = '\r';
            expected[utf8.size() + 1] = '\n';
        }
        const auto name = "sample-" + std::to_string(size) + ".bin";
        tree.file(fs::path("selected-assets") / name, expected);
        const auto result = app::readAssetFile(paths, name);
        REQUIRE_FALSE(result.error.has_value());
        REQUIRE(result.bytes.size() == expected.size());
        REQUIRE(result.bytes == expected);
    }
}

TEST_CASE("Unicode resource paths are opened as native paths without narrowing",
          "[asset-resolver]") {
    TempTree tree;
    const auto rootName = fs::path(u8"çalışma alanı/资源目录");
    const auto paths = assetPaths(tree, rootName);
    const auto resourceName = fs::path(u8"yazı tipleri/字体 🎮.bin");
    const auto expectedPath = tree.file(rootName / resourceName, "native Unicode file bytes\r\n");

    requireResolved(app::resolveAssetPath(paths, resourceName), expectedPath);
    const auto result = app::readAssetFile(paths, resourceName);
    REQUIRE_FALSE(result.error.has_value());
    REQUIRE(result.bytes == "native Unicode file bytes\r\n");
}

TEST_CASE(
    "repeated resource resolution and reads leave content entries and modification times intact",
    "[asset-resolver]") {
    TempTree tree;
    const auto paths = assetPaths(tree);
    const auto font = tree.file("selected-assets/fonts/font.ttf", "existing bytes\r\n");
    tree.file("selected-assets/sounds/merge.wav", "untouched sound bytes");
    tree.file("unrelated/profile/scores.json", "untouched profile bytes");
    const auto before = snapshotTree(tree.root());

    for (int iteration = 0; iteration < 8; ++iteration) {
        INFO("Read: " << iteration);
        requireResolved(app::resolveAssetPath(paths, fs::path("fonts") / "font.ttf"), font);
        const auto read = app::readAssetFile(paths, fs::path("fonts") / "font.ttf");
        REQUIRE_FALSE(read.error.has_value());
        REQUIRE(read.bytes == "existing bytes\r\n");
        requireReadError(app::readAssetFile(paths, "missing.bin"), AssetErrorCode::Missing,
                         paths.directory() / "missing.bin");
        REQUIRE(snapshotTree(tree.root()) == before);
    }
}

TEST_CASE("a file in an asset parent component reports a filesystem error without fallback",
          "[asset-resolver]") {
    TempTree tree;
    const auto paths = assetPaths(tree);
    tree.file("selected-assets/fonts", "directory obstruction");
    tree.file("other-assets/fonts/font.ttf", "fallback must not be read");
    const auto candidate = paths.directory() / "fonts" / "font.ttf";
    const auto before = snapshotTree(tree.root());
    const auto result = app::resolveAssetPath(paths, fs::path("fonts") / "font.ttf");
    REQUIRE_FALSE(result.resolvedPath.has_value());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->path == candidate);
#ifndef _WIN32
    REQUIRE(result.error->code == AssetErrorCode::IoError);
    REQUIRE(result.error->systemError == std::errc::not_a_directory);
#else
    REQUIRE((result.error->code == AssetErrorCode::IoError ||
             result.error->code == AssetErrorCode::Missing));
#endif
    const auto read = app::readAssetFile(paths, fs::path("fonts") / "font.ttf");
    requireReadError(read, result.error->code, candidate);
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("a selected root replaced after selection cannot trigger another asset root",
          "[asset-resolver]") {
    TempTree tree;
    const auto paths = assetPaths(tree);
    tree.file("other-assets/font.ttf", "alternative root must not be read");
    fs::remove(paths.directory());
    tree.file("selected-assets", "root obstruction");
    const auto before = snapshotTree(tree.root());
    const auto resolution = app::resolveAssetPath(paths, "font.ttf");
    REQUIRE_FALSE(resolution.resolvedPath.has_value());
    REQUIRE(resolution.error.has_value());
    REQUIRE(resolution.error->path == paths.directory() / "font.ttf");
    requireReadError(app::readAssetFile(paths, "font.ttf"), resolution.error->code,
                     paths.directory() / "font.ttf");
    REQUIRE(snapshotTree(tree.root()) == before);
}

#ifdef _WIN32
TEST_CASE("Windows rooted resource names are rejected even without an absolute drive path",
          "[asset-resolver][windows]") {
    TempTree tree;
    const auto paths = assetPaths(tree);
    const std::array<fs::path, 4> rootedNames{
        fs::path(L"C:relative-font.ttf"), fs::path(L"\\root-relative-font.ttf"),
        fs::path(L"C:\\absolute-font.ttf"), fs::path(L"\\\\server\\share\\font.ttf")};
    for (const auto &name : rootedNames) {
        requireError(app::resolveAssetPath(paths, name), AssetErrorCode::AbsolutePath, name);
        requireReadError(app::readAssetFile(paths, name), AssetErrorCode::AbsolutePath, name);
    }
}
#endif

#ifndef _WIN32
TEST_CASE("trusted asset symlinks retain their selected names and can be read",
          "[asset-resolver][symlink]") {
    TempTree tree;
    const auto paths = assetPaths(tree);
    const auto externalFont = tree.file("outside/font.bin", "linked font bytes\r\n");
    const auto externalDirectory = tree.directory("outside/sounds");
    const auto fontLink = paths.directory() / "font.bin";
    const auto directoryLink = paths.directory() / "sounds";
    assetSymlink(externalFont, fontLink);
    assetSymlink(externalDirectory, directoryLink);

    requireResolved(app::resolveAssetPath(paths, "font.bin"), fontLink);
    requireResolved(app::resolveAssetPath(paths, "sounds", AssetKind::Directory), directoryLink);
    requireError(app::resolveAssetPath(paths, "sounds"), AssetErrorCode::WrongType, directoryLink);
    const auto read = app::readAssetFile(paths, "font.bin");
    REQUIRE_FALSE(read.error.has_value());
    REQUIRE(read.bytes == "linked font bytes\r\n");
    REQUIRE(fs::read_symlink(fontLink) == externalFont);
}

TEST_CASE("dangling asset links are reported as missing while the link remains untouched",
          "[asset-resolver][symlink]") {
    TempTree tree;
    const auto paths = assetPaths(tree);
    const auto target = tree.root() / "missing-target";
    const auto link = paths.directory() / "font.ttf";
    assetSymlink(target, link);

    requireError(app::resolveAssetPath(paths, "font.ttf"), AssetErrorCode::Missing, link);
    requireReadError(app::readAssetFile(paths, "font.ttf"), AssetErrorCode::Missing, link);
    REQUIRE(fs::read_symlink(link) == target);
    REQUIRE_FALSE(fs::exists(target));
}
#endif
