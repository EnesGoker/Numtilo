#include "app/AssetLocations.hpp"
#include "app/ProfileMigration.hpp"
#include "app/SettingsStore.hpp"
#include "app/UserDataPaths.hpp"
#include "core/ScoreManager.hpp"
#include "core/detail/FileSnapshot.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;
using app::ProfileImportStatus;
using numtilo_core::FileSaveStatus;

constexpr std::string_view kMarkerName = ".numtilo-acceptance";
constexpr std::string_view kMarkerBytes = "numtilo-acceptance-v1\n";
constexpr std::string_view kAcceptedRound = "accept-round";
constexpr int kAcceptedBest = 60000;

void require(bool condition, std::string_view reason) {
    if (!condition) {
        throw std::runtime_error(std::string(reason));
    }
}

std::string pathUtf8(const fs::path &path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char *>(text.data()), text.size()};
}

std::vector<fs::path> nativeArguments(int argc, char *argv[]) {
    std::vector<fs::path> arguments;
#ifdef _WIN32
    // CRT narrow argv can lose Unicode paths. Read the native command line instead.
    static_cast<void>(argc);
    static_cast<void>(argv);
    int wideCount = 0;
    wchar_t **wideArguments = CommandLineToArgvW(GetCommandLineW(), &wideCount);
    if (!wideArguments) {
        throw std::system_error(
            std::error_code(static_cast<int>(GetLastError()), std::system_category()),
            "read native worker arguments");
    }
    struct FreeArguments {
        void operator()(wchar_t **value) const noexcept {
            LocalFree(value);
        }
    };
    const std::unique_ptr<wchar_t *, FreeArguments> ownedArguments(wideArguments);
    for (int index = 0; index < wideCount; ++index) {
        arguments.emplace_back(wideArguments[index]);
    }
#else
    for (int index = 0; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
#endif
    return arguments;
}

struct Options {
    fs::path sandbox;
    fs::path profile;
    fs::path legacy;
    std::string phase;
};

Options parseOptions(const std::vector<fs::path> &arguments) {
    Options options;
    std::array<bool, 4> supplied{};
    for (std::size_t index = 1; index < arguments.size(); index += 2) {
        require(index + 1 < arguments.size(), "every worker option requires a value");
        const auto &key = arguments[index];
        const auto &value = arguments[index + 1];
        std::size_t slot;
        if (key == "--sandbox") {
            slot = 0;
            options.sandbox = value;
        } else if (key == "--profile") {
            slot = 1;
            options.profile = value;
        } else if (key == "--legacy") {
            slot = 2;
            options.legacy = value;
        } else if (key == "--phase") {
            slot = 3;
            options.phase = pathUtf8(value);
        } else {
            throw std::runtime_error("unknown worker option");
        }
        require(!supplied[slot], "worker options cannot be supplied twice");
        supplied[slot] = true;
    }
    require(std::all_of(supplied.begin(), supplied.end(), [](bool value) { return value; }),
            "required options: --sandbox --profile --legacy --phase");
    require(options.phase == "write" || options.phase == "reopen" || options.phase == "blocked" ||
                options.phase == "recover",
            "unknown acceptance phase");
    return options;
}

void requireAbsolutePath(const fs::path &path) {
    require(!path.empty() && path.is_absolute(), "worker roots must be nonempty absolute paths");
    require(path.native().find(fs::path::value_type{}) == fs::path::string_type::npos,
            "worker roots cannot contain NUL");
    for (const auto &component : path) {
        require(component != "..", "worker roots cannot traverse a parent directory");
    }
}

bool strictChild(const fs::path &path, const fs::path &parent) {
    const auto relative = path.lexically_relative(parent);
    if (relative.empty() || relative == "." || relative.has_root_path()) {
        return false;
    }
    return std::none_of(relative.begin(), relative.end(),
                        [](const fs::path &component) { return component == ".."; });
}

bool missing(const fs::file_status &status, const std::error_code &error) {
    return status.type() == fs::file_type::not_found &&
           (!error || error == std::errc::no_such_file_or_directory);
}

fs::path validateChildRoot(const fs::path &input, const fs::path &sandboxInput,
                           const fs::path &canonicalSandbox) {
    requireAbsolutePath(input);
    const auto relative = input.lexically_relative(sandboxInput);
    require(strictChild(input, sandboxInput), "profile and legacy roots must be sandbox children");
    auto probe = canonicalSandbox;
    for (const auto &component : relative) {
        probe /= component;
        std::error_code error;
        const auto status = fs::symlink_status(probe, error);
        if (missing(status, error)) {
            continue;
        }
        require(!error && fs::is_directory(status) && !fs::is_symlink(status),
                "worker roots require directory ancestors without inside-sandbox symlinks");
    }
    const auto checked = fs::weakly_canonical(input);
    require(strictChild(checked, canonicalSandbox), "worker root escaped its canonical sandbox");
    return checked;
}

void rejectLeafSymlinks(const fs::path &directory) {
    for (const auto *name :
         {"scores.json", "settings.json", "scores.json.bak", "settings.json.bak"}) {
        std::error_code error;
        const auto status = fs::symlink_status(directory / name, error);
        if (missing(status, error)) {
            continue;
        }
        require(!error && !fs::is_symlink(status), "worker profile leaves must not be symlinks");
    }
}

void validateIsolation(const Options &options) {
    requireAbsolutePath(options.sandbox);
    std::error_code error;
    const auto sandboxStatus = fs::symlink_status(options.sandbox, error);
    require(!error && fs::is_directory(sandboxStatus) && !fs::is_symlink(sandboxStatus),
            "worker sandbox must be an existing directory rather than a symlink");
    const auto canonicalSandbox = fs::canonical(options.sandbox);
    const auto marker = canonicalSandbox / kMarkerName;
    const auto markerStatus = fs::symlink_status(marker, error);
    require(!error && fs::is_regular_file(markerStatus) && !fs::is_symlink(markerStatus),
            "worker sandbox requires its regular acceptance marker");
    const auto snapshot = numtilo_core::detail::readFileSnapshot(marker);
    require(snapshot.status == numtilo_core::detail::FileReadStatus::Read &&
                snapshot.bytes == kMarkerBytes,
            "worker acceptance marker content is invalid");
    const auto profile = validateChildRoot(options.profile, options.sandbox, canonicalSandbox);
    const auto legacy = validateChildRoot(options.legacy, options.sandbox, canonicalSandbox);
    require(profile != legacy && !strictChild(profile, legacy) && !strictChild(legacy, profile),
            "profile and legacy roots must be disjoint sandbox children");
    rejectLeafSymlinks(profile);
    rejectLeafSymlinks(legacy);
}

struct FileSnapshot {
    bool present{false};
    std::string bytes;
    std::optional<fs::file_time_type> modified;

    bool operator==(const FileSnapshot &) const = default;
};

FileSnapshot captureFile(const fs::path &path) {
    const auto read = numtilo_core::detail::readFileSnapshot(path);
    if (read.status == numtilo_core::detail::FileReadStatus::Missing) {
        return {};
    }
    require(read.status == numtilo_core::detail::FileReadStatus::Read,
            "acceptance fixtures require readable regular profile files");
    return {true, read.bytes, fs::last_write_time(path)};
}

std::array<FileSnapshot, 4> captureProfileFiles(const fs::path &directory) {
    return {captureFile(directory / "scores.json"), captureFile(directory / "settings.json"),
            captureFile(directory / "scores.json.bak"),
            captureFile(directory / "settings.json.bak")};
}

std::string importStatusName(ProfileImportStatus status) {
    switch (status) {
    case ProfileImportStatus::DestinationPresent:
        return "DestinationPresent";
    case ProfileImportStatus::SourceMissing:
        return "SourceMissing";
    case ProfileImportStatus::Imported:
        return "Imported";
    case ProfileImportStatus::InvalidLegacyLocation:
        return "InvalidLegacyLocation";
    case ProfileImportStatus::DestinationIoError:
        return "DestinationIoError";
    case ProfileImportStatus::SourceIoError:
        return "SourceIoError";
    case ProfileImportStatus::InvalidSource:
        return "InvalidSource";
    case ProfileImportStatus::UnsupportedVersion:
        return "UnsupportedVersion";
    case ProfileImportStatus::SourceChanged:
        return "SourceChanged";
    case ProfileImportStatus::PublicationFailed:
        return "PublicationFailed";
    }
    throw std::runtime_error("unrecognized import status");
}

std::string saveStatusName(FileSaveStatus status) {
    switch (status) {
    case FileSaveStatus::NotSaved:
        return "NotSaved";
    case FileSaveStatus::Saved:
        return "Saved";
    case FileSaveStatus::WriteBlocked:
        return "WriteBlocked";
    case FileSaveStatus::SerializationError:
        return "SerializationError";
    case FileSaveStatus::SourceChanged:
        return "SourceChanged";
    case FileSaveStatus::BackupConflict:
        return "BackupConflict";
    case FileSaveStatus::IoError:
        return "IoError";
    }
    throw std::runtime_error("unrecognized save status");
}

std::size_t acceptanceRoundCount(const numtilo_core::ScoreManager &scores) {
    return static_cast<std::size_t>(
        std::count_if(scores.completedGames().begin(), scores.completedGames().end(),
                      [](const auto &entry) { return entry.gameId == kAcceptedRound; }));
}

void verifyPersistedProfile(const app::UserDataPaths &paths) {
    numtilo_core::ScoreManager scores(paths.scoresFile());
    app::SettingsStore settings(paths.settingsFile());
    require(scores.load() && settings.load(), "saved profile could not be reopened");
    require(scores.bestScore() == kAcceptedBest && scores.completedGames().size() == 3 &&
                acceptanceRoundCount(scores) == 1,
            "persisted score history or best score differs from acceptance expectations");
    require(!settings.soundEnabled(),
            "persisted sound preference differs from acceptance expectation");
    require(!scores.isDirty() && !settings.isDirty(), "canonical reopens must be clean");
}

Json runAcceptance(const Options &options) {
    validateIsolation(options);
    // Native queries are smoke checks only; none of their locations is used for bootstrap or
    // writes.
    const auto nativeAssets = app::discoverAssetLocations();
    require(!nativeAssets.error && (nativeAssets.executableDir || nativeAssets.bundleResourcesDir),
            "native asset location discovery failed");
    for (const auto &path : {nativeAssets.executableDir, nativeAssets.bundleResourcesDir}) {
        require(!path || (!path->empty() && path->is_absolute()),
                "native asset location is invalid");
    }
    const auto nativeData = app::discoverUserDataBase();
    require(!nativeData.error && nativeData.base && !nativeData.base->empty() &&
                nativeData.base->is_absolute(),
            "native data-base discovery failed");

    app::PathOptions pathOptions;
    pathOptions.userDataOverride = options.profile;
    const auto resolution = app::resolveUserDataPaths({}, pathOptions);
    require(resolution.paths.has_value() && !resolution.error,
            "explicit fixture profile did not resolve");
    const auto &paths = *resolution.paths;
    require(paths.directory() == options.profile, "fixture profile override was changed");
    const auto sourcesBefore = captureProfileFiles(options.legacy);
    const auto targetsBefore = captureProfileFiles(paths.directory());
    const auto directory = app::ensureUserDataDirectory(paths);
    require(directory.ready && !directory.error, "fixture profile directory bootstrap failed");
    const auto imported = app::migrateLegacyProfile(paths, {options.legacy, {}});
    numtilo_core::ScoreManager scores(paths.scoresFile(),
                                      app::storageAfterProfileImport(imported.scores));
    app::SettingsStore settings(paths.settingsFile(),
                                app::storageAfterProfileImport(imported.settings));
    require(scores.load() && settings.load(), "fixture profile loaders failed");
    const bool scoreLegacy = scores.needsMigration();
    const bool settingsLegacy = settings.needsMigration();

    if (options.phase == "reopen") {
        require(imported.scores.status == ProfileImportStatus::DestinationPresent &&
                    imported.settings.status == ProfileImportStatus::DestinationPresent,
                "reopen must preserve authoritative existing profile targets");
        verifyPersistedProfile(paths);
        require(captureProfileFiles(paths.directory()) == targetsBefore,
                "reopening modified canonical profile files or backups");
    } else {
        if (options.phase == "blocked") {
            require(imported.scores.status == ProfileImportStatus::InvalidSource ||
                        imported.scores.status == ProfileImportStatus::UnsupportedVersion,
                    "blocked phase requires an invalid or unsupported legacy score source");
            require(!imported.scores.allowsProfileWrites() && !targetsBefore[0].present,
                    "blocked phase must protect an absent canonical score target");
            require(imported.settings.allowsProfileWrites(),
                    "score failure blocked independent settings");
        } else {
            require(imported.scores.allowsProfileWrites() &&
                        imported.settings.allowsProfileWrites(),
                    "valid source profile import was blocked");
            require(
                scores.completedGames().size() == 2 ||
                    (scores.completedGames().size() == 3 && acceptanceRoundCount(scores) == 1),
                "write fixtures must contain two legacy rounds or a previously accepted profile");
        }
        scores.updateBestScore(kAcceptedBest);
        scores.recordCompletedGame(std::string(kAcceptedRound), kAcceptedBest, "Acceptance player",
                                   "2026-10-06T12:00:00Z");
        settings.setSoundEnabled(false);
        if (options.phase == "blocked") {
            require(!scores.save() && scores.lastSaveStatus() == FileSaveStatus::WriteBlocked,
                    "unresolved import allowed an empty-profile overwrite");
            require(!scores.save() && scores.isDirty(),
                    "write barrier or dirty memory was lost on retry");
            require(!fs::exists(paths.scoresFile()),
                    "blocked score save created a canonical target");
            require(scores.completedGames().size() == 1 && acceptanceRoundCount(scores) == 1,
                    "blocked memory state lost its unsaved round");
        } else if (scores.isDirty() || scores.needsMigration()) {
            require(scores.save(), "acceptance score save failed");
        }
        if (settings.isDirty() || settings.needsMigration()) {
            require(settings.save(), "independent settings save failed");
        }
        require(scores.bestScore() == kAcceptedBest,
                "best-score acceptance value was not preserved");
        require(!settings.soundEnabled(), "acceptance sound preference was not preserved");
        if (options.phase != "blocked") {
            verifyPersistedProfile(paths);
        }
    }
    require(captureProfileFiles(options.legacy) == sourcesBefore,
            "profile import or ordinary saves modified legacy source files or backups");
    if (scoreLegacy && imported.scores.status == ProfileImportStatus::Imported) {
        auto backup = paths.scoresFile();
        backup += ".bak";
        require(captureFile(backup).bytes == sourcesBefore[0].bytes,
                "score format conversion backup differs from the imported legacy bytes");
    }
    if (settingsLegacy && imported.settings.status == ProfileImportStatus::Imported) {
        auto backup = paths.settingsFile();
        backup += ".bak";
        require(captureFile(backup).bytes == sourcesBefore[1].bytes,
                "settings format conversion backup differs from the imported legacy bytes");
    }

    return {{"version", 1},
            {"phase", options.phase},
            {"success", true},
            {"profile_dir", pathUtf8(paths.directory())},
            {"scores_file", pathUtf8(paths.scoresFile())},
            {"settings_file", pathUtf8(paths.settingsFile())},
            {"best_score", scores.bestScore()},
            {"completed_count", scores.completedGames().size()},
            {"accept_round_count", acceptanceRoundCount(scores)},
            {"sound_enabled", settings.soundEnabled()},
            {"score_dirty", scores.isDirty()},
            {"settings_dirty", settings.isDirty()},
            {"scores_file_exists", fs::exists(paths.scoresFile())},
            {"score_save_status", saveStatusName(scores.lastSaveStatus())},
            {"settings_save_status", saveStatusName(settings.lastSaveStatus())},
            {"scores_import_status", importStatusName(imported.scores.status)},
            {"settings_import_status", importStatusName(imported.settings.status)},
            {"native_asset_locations_valid", true},
            {"native_user_data_base_valid", true}};
}

} // namespace

int main(int argc, char *argv[]) {
    Json report;
    try {
        const auto options = parseOptions(nativeArguments(argc, argv));
        report = runAcceptance(options);
    } catch (const std::exception &error) {
        report = {{"version", 1}, {"success", false}, {"error", error.what()}};
    }
    std::cout << report.dump(-1, ' ', false, Json::error_handler_t::replace) << '\n';
    return report.value("success", false) ? 0 : 1;
}
