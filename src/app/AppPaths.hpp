#pragma once

#include <filesystem>
#include <optional>
#include <system_error>

namespace app {

// Overrides name the final application data / assets directory, not their parent.
// A supplied override is authoritative, including when it is invalid.
struct PathOptions {
    std::optional<std::filesystem::path> userDataOverride;
    std::optional<std::filesystem::path> assetOverride;
};

// Native platform adapters provide these locations; resolution never reads
// environment variables, discovers the executable or consults the working directory.
struct PathContext {
    std::optional<std::filesystem::path> userDataBase;
    std::optional<std::filesystem::path> executableDir;
    std::optional<std::filesystem::path> bundleResourcesDir;
};

enum class PathSource { Override, PlatformUserData, BundleResources, ExecutableDirectory };
enum class PathKind { UserData, Assets, ExecutableDirectory };
enum class PathErrorCode {
    MissingLocation,
    EmptyPath,
    RelativePath,
    EmbeddedNull,
    NotDirectory,
    IoError
};

struct PathError {
    PathKind kind;
    PathErrorCode code;
    std::filesystem::path path;
    std::error_code systemError;
};

struct PathResolution;
struct UserDataResolution;
struct AssetPathResolution;

// Profile paths can be resolved without locating or validating any assets.
class UserDataPaths {
  public:
    const std::filesystem::path &directory() const noexcept;
    std::filesystem::path scoresFile() const;
    std::filesystem::path settingsFile() const;
    PathSource source() const noexcept;

  private:
    friend UserDataResolution resolveUserDataPaths(const PathContext &, const PathOptions &);
    UserDataPaths(std::filesystem::path directory, PathSource source);
    std::filesystem::path directory_;
    PathSource source_;
};

struct UserDataResolution {
    std::optional<UserDataPaths> paths;
    std::optional<PathError> error;
};

// Same user-data rules as resolveAppPaths, independent of the asset inputs.
UserDataResolution resolveUserDataPaths(const PathContext &context,
                                        const PathOptions &options = {});

// Asset selection is independent of user-data discovery, validation and bootstrap.
class AssetPaths {
  public:
    const std::filesystem::path &directory() const noexcept;
    const std::optional<std::filesystem::path> &executableDir() const noexcept;
    PathSource source() const noexcept;

  private:
    friend AssetPathResolution resolveAssetPaths(const PathContext &, const PathOptions &);
    AssetPaths(std::filesystem::path directory, std::optional<std::filesystem::path> executableDir,
               PathSource source);
    std::filesystem::path directory_;
    std::optional<std::filesystem::path> executableDir_;
    PathSource source_;
};

struct AssetPathResolution {
    std::optional<AssetPaths> paths;
    std::optional<PathError> error;
};

AssetPathResolution resolveAssetPaths(const PathContext &context, const PathOptions &options = {});

// A successfully resolved snapshot: writable profile data and read-only assets
// have separate roots. No resolver operation creates directories or writes files.
class AppPaths {
  public:
    const std::filesystem::path &userDataDir() const noexcept;
    const std::filesystem::path &assetDir() const noexcept;
    // Present only when assets use the executable-relative layout. Unused context
    // fields are neither validated nor exposed as resolved locations.
    const std::optional<std::filesystem::path> &executableDir() const noexcept;
    std::filesystem::path scoresFile() const;
    std::filesystem::path settingsFile() const;
    PathSource userDataSource() const noexcept;
    PathSource assetSource() const noexcept;

  private:
    friend PathResolution resolveAppPaths(const PathContext &, const PathOptions &);
    AppPaths(UserDataPaths userData, AssetPaths assets);

    UserDataPaths userData_;
    AssetPaths assets_;
};

// Exactly one alternative is populated by resolveAppPaths. On failure no partial
// AppPaths is exposed; the error identifies the selected location and native cause.
struct PathResolution {
    std::optional<AppPaths> paths;
    std::optional<PathError> error;
};

// Selected input paths must be absolute, nonempty and contain no embedded NUL.
// Assets: override > bundleResourcesDir/assets > executableDir/assets. No fallback
// after selecting a root. User data: override > userDataBase/numtilo.
// Assets and the selected executable directory must exist as directories. User data
// may be absent; this does not verify write permissions or create it. Symlinks are
// allowed and kept as supplied (no lexical '..' collapse across symlink boundaries).
PathResolution resolveAppPaths(const PathContext &context, const PathOptions &options = {});

} // namespace app
