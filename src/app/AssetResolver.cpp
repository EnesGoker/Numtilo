#include "app/AssetResolver.hpp"

#include <array>
#include <fstream>
#include <utility>

namespace app {
namespace {

AssetResolution failure(AssetErrorCode code, const std::filesystem::path &path,
                        std::error_code error = {}) {
    return {std::nullopt, AssetError{code, path, error}};
}

} // namespace

AssetResolution resolveAssetPath(const AssetPaths &paths, const std::filesystem::path &relativePath,
                                 AssetKind kind) {
    if (relativePath.empty()) {
        return failure(AssetErrorCode::EmptyPath, relativePath);
    }
    if (relativePath.native().find(std::filesystem::path::value_type{}) !=
        std::filesystem::path::string_type::npos) {
        return failure(AssetErrorCode::EmbeddedNull, relativePath);
    }
    if (relativePath.has_root_path()) {
        return failure(AssetErrorCode::AbsolutePath, relativePath);
    }
    for (const auto &component : relativePath) {
        if (component == "..") {
            return failure(AssetErrorCode::ParentTraversal, relativePath);
        }
    }
    const auto path = paths.directory() / relativePath;
    std::error_code error;
    const auto status = std::filesystem::status(path, error);
    if (status.type() == std::filesystem::file_type::not_found &&
        (!error || error == std::errc::no_such_file_or_directory)) {
        return failure(AssetErrorCode::Missing, path, error);
    }
    if (error) {
        return failure(AssetErrorCode::IoError, path, error);
    }
    const bool expectedType = kind == AssetKind::Directory
                                  ? std::filesystem::is_directory(status)
                                  : std::filesystem::is_regular_file(status);
    if (!expectedType) {
        return failure(AssetErrorCode::WrongType, path);
    }
    return {path, std::nullopt};
}

AssetReadResult readAssetFile(const AssetPaths &paths, const std::filesystem::path &relativePath) {
    auto resolution = resolveAssetPath(paths, relativePath);
    if (resolution.error) {
        return {{}, std::move(resolution.error)};
    }
    const auto &path = *resolution.resolvedPath;
    std::ifstream input(path, std::ios::binary);
    const auto readFailure = [&path] {
        return AssetReadResult{
            {},
            AssetError{AssetErrorCode::IoError, path, std::make_error_code(std::errc::io_error)}};
    };
    if (!input.is_open()) {
        return readFailure();
    }
    std::string bytes;
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        bytes.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
    }
    if (input.bad() || !input.eof()) {
        return readFailure();
    }
    input.clear();
    input.close();
    if (input.fail()) {
        return readFailure();
    }
    return {std::move(bytes), std::nullopt};
}

} // namespace app
