#include "app/detail/SettingsValidation.hpp"

#include "core/detail/ProfileValidation.hpp"

#include <cstdint>

namespace app::detail {

ParsedSettingsProfile parseSettingsProfile(const std::string &bytes) {
    ParsedSettingsProfile result;
    const auto parsedRoot = numtilo_core::detail::parseUniqueJson(bytes);
    if (!parsedRoot || !parsedRoot->is_object()) {
        return result;
    }
    const auto &root = *parsedRoot;
    const bool legacy = !root.contains("version");
    if (!legacy) {
        const auto &version = root["version"];
        if (!version.is_number_integer()) {
            return result;
        }
        const bool supported = version.is_number_unsigned()
                                   ? version.get<std::uint64_t>() == SettingsStore::kFormatVersion
                                   : version.get<std::int64_t>() == SettingsStore::kFormatVersion;
        if (!supported) {
            result.status = SettingsLoadStatus::UnsupportedVersion;
            return result;
        }
    }
    if (root.size() != (legacy ? 1U : 2U) || !root.contains("sound_enabled") ||
        !root["sound_enabled"].is_boolean()) {
        return result;
    }
    result.soundEnabled = root["sound_enabled"].get<bool>();
    result.status =
        legacy ? SettingsLoadStatus::LegacyMigrationPending : SettingsLoadStatus::LoadedV1;
    return result;
}

SettingsLoadStatus validateSettingsProfile(const std::string &bytes) {
    return parseSettingsProfile(bytes).status;
}

} // namespace app::detail
