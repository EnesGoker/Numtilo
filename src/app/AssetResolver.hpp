#pragma once

#include "app/AppPaths.hpp"

#include <string>

namespace app {

enum class AssetKind { File, Directory };
enum class AssetErrorCode {
    EmptyPath,
    AbsolutePath,
    EmbeddedNull,
    ParentTraversal,
    Missing,
    WrongType,
    IoError
};

struct AssetError {
    AssetErrorCode code;
    std::filesystem::path path;
    std::error_code systemError;
};

struct AssetResolution {
    std::optional<std::filesystem::path> resolvedPath;
    std::optional<AssetError> error;
};

// Resource names are relative to the selected assets directory, without an
// "assets/" prefix. No CWD, source-tree or executable-parent fallback is used.
// Empty/rooted/NUL/parent-traversal inputs are rejected before filesystem access.
// Directory/file symlinks are allowed as for AppPaths; this is not a sandbox.
AssetResolution resolveAssetPath(const AssetPaths &paths, const std::filesystem::path &relativePath,
                                 AssetKind kind = AssetKind::File);

struct AssetReadResult {
    std::string bytes;
    std::optional<AssetError> error;
};

// Open through std::filesystem::path, preserving native Unicode paths on Windows.
// SFML decoders receive these bytes rather than a narrowed filename. Resolution
// is a snapshot; an actual open/read failure remains a reported asset error.
AssetReadResult readAssetFile(const AssetPaths &paths, const std::filesystem::path &relativePath);

} // namespace app
