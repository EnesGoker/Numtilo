#include "core/detail/ProfileValidation.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <unordered_set>
#include <utility>

namespace numtilo_core::detail {
namespace {

using Json = nlohmann::json;

bool hasExactKeys(const Json &object, std::initializer_list<const char *> keys) {
    return object.is_object() && object.size() == keys.size() &&
           std::all_of(keys.begin(), keys.end(),
                       [&object](const char *key) { return object.contains(key); });
}

std::optional<int> parseScore(const Json &score) {
    if (!score.is_number_integer()) {
        return std::nullopt;
    }
    if (score.is_number_unsigned()) {
        if (score.get<std::uint64_t>() >
            static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            return std::nullopt;
        }
    } else if (score.get<std::int64_t>() < 0 ||
               score.get<std::int64_t>() > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }
    return score.get<int>();
}

std::optional<ScoreEntry> parseEntry(const Json &item, const bool legacy) {
    if (!item.is_object() || !item.contains("score") || !item.contains("played_at") ||
        !item["played_at"].is_string()) {
        return std::nullopt;
    }
    const auto score = parseScore(item["score"]);
    if (!score) {
        return std::nullopt;
    }

    ScoreEntry entry;
    entry.score = *score;
    entry.playedAtUtc = item["played_at"].get<std::string>();
    entry.playerName = "Oyuncu";
    if (item.contains("player_name")) {
        if (!item["player_name"].is_string()) {
            return std::nullopt;
        }
        const auto candidate = item["player_name"].get<std::string>();
        if (!candidate.empty()) {
            entry.playerName = candidate;
        } else if (!legacy) {
            return std::nullopt;
        }
    }

    if (!legacy) {
        if (!hasExactKeys(item, {"game_id", "score", "played_at", "player_name"}) ||
            !item["game_id"].is_string()) {
            return std::nullopt;
        }
        entry.gameId = item["game_id"].get<std::string>();
        if (entry.gameId.empty()) {
            return std::nullopt;
        }
    }

    return entry;
}

} // namespace

std::optional<nlohmann::json> parseUniqueJson(const std::string &bytes) {
    // JSON forbids literal NUL bytes. The lexer treats NUL as end-of-input, so
    // rejecting it here prevents accepting a valid prefix while ignoring a suffix.
    // An escaped \\u0000 remains valid JSON and may decode into string metadata.
    if (bytes.find('\0') != std::string::npos) {
        return std::nullopt;
    }
    bool duplicateKeys = false;
    std::vector<std::unordered_set<std::string>> objectKeys;
    const auto checkKeys = [&objectKeys, &duplicateKeys](int, const Json::parse_event_t event,
                                                         Json &value) {
        if (event == Json::parse_event_t::object_start) {
            objectKeys.emplace_back();
        } else if (event == Json::parse_event_t::key) {
            if (!objectKeys.back().insert(value.get<std::string>()).second) {
                duplicateKeys = true;
            }
        } else if (event == Json::parse_event_t::object_end) {
            objectKeys.pop_back();
        }
        return true;
    };
    try {
        auto root = Json::parse(bytes, checkKeys);
        if (!duplicateKeys) {
            return root;
        }
    } catch (const Json::exception &) {
        // Malformed input is a validation result, not a filesystem failure.
    }
    return std::nullopt;
}

ParsedScoreProfile parseScoreProfile(const std::string &bytes) {
    ParsedScoreProfile result;
    const auto parsedRoot = parseUniqueJson(bytes);
    if (!parsedRoot || !parsedRoot->is_object()) {
        return result;
    }
    const auto &root = *parsedRoot;
    const bool legacy = !root.contains("version");
    const Json *games = nullptr;
    std::optional<int> storedBest;
    if (legacy) {
        if (!hasExactKeys(root, {"scores"}) || !root["scores"].is_array()) {
            return result;
        }
        games = &root["scores"];
    } else {
        const auto &version = root["version"];
        if (!version.is_number_integer()) {
            return result;
        }
        const bool supported = version.is_number_unsigned()
                                   ? version.get<std::uint64_t>() == ScoreManager::kFormatVersion
                                   : version.get<std::int64_t>() == ScoreManager::kFormatVersion;
        if (!supported) {
            result.status = ScoreLoadStatus::UnsupportedVersion;
            return result;
        }
        if (!hasExactKeys(root, {"version", "best_score", "completed_games"}) ||
            !root["completed_games"].is_array()) {
            return result;
        }
        storedBest = parseScore(root["best_score"]);
        if (!storedBest) {
            return result;
        }
        games = &root["completed_games"];
    }

    std::vector<ScoreEntry> loadedGames;
    std::unordered_set<std::string> gameIds;
    int completedBest = 0;
    for (const auto &item : *games) {
        auto parsed = parseEntry(item, legacy);
        if (!parsed) {
            return result;
        }
        if (legacy) {
            parsed->gameId = "legacy-" + std::to_string(loadedGames.size() + 1);
        }
        if (!gameIds.insert(parsed->gameId).second) {
            return result;
        }
        completedBest = std::max(completedBest, parsed->score);
        loadedGames.push_back(std::move(*parsed));
    }
    if (storedBest && *storedBest < completedBest) {
        return result;
    }

    result.completedGames = std::move(loadedGames);
    result.bestScore = storedBest.value_or(completedBest);
    result.status = legacy ? ScoreLoadStatus::LegacyMigrationPending : ScoreLoadStatus::LoadedV2;
    return result;
}

ScoreLoadStatus validateScoreProfile(const std::string &bytes) {
    return parseScoreProfile(bytes).status;
}

} // namespace numtilo_core::detail
