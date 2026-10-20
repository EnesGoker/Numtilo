#pragma once

#include "app/SettingsStore.hpp"

#include <string>

namespace app::detail {

struct ParsedSettingsProfile {
    SettingsLoadStatus status{SettingsLoadStatus::InvalidData};
    bool soundEnabled{true};
};

ParsedSettingsProfile parseSettingsProfile(const std::string &bytes);
SettingsLoadStatus validateSettingsProfile(const std::string &bytes);

} // namespace app::detail
