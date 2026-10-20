#pragma once

#include "core/ScoreManager.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace numtilo_core::detail {

struct ParsedScoreProfile {
    ScoreLoadStatus status{ScoreLoadStatus::InvalidData};
    int bestScore{0};
    std::vector<ScoreEntry> completedGames;
};

// Pure parsing of the supplied bytes. Failure exposes no partially parsed model.
ParsedScoreProfile parseScoreProfile(const std::string &bytes);
ScoreLoadStatus validateScoreProfile(const std::string &bytes);

// Shared JSON syntax validation rejects duplicate keys at every object depth.
std::optional<nlohmann::json> parseUniqueJson(const std::string &bytes);

} // namespace numtilo_core::detail
