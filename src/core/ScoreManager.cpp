#include "core/ScoreManager.hpp"

#include "core/detail/FileSnapshot.hpp"
#include "core/detail/ProfileValidation.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace numtilo_core {

namespace {

using Json = nlohmann::json;

Json toJson(const ScoreEntry &entry) {
    Json item;
    item["score"] = entry.score;
    item["played_at"] = entry.playedAtUtc;
    item["player_name"] = entry.playerName;
    item["game_id"] = entry.gameId;
    return item;
}

} // namespace

ScoreManager::ScoreManager(std::filesystem::path scoreFilePath,
                           std::shared_ptr<ScoreStorage> storage)
    : scoreFilePath_(std::move(scoreFilePath)),
      storage_(storage ? std::move(storage) : makeScoreStorage()) {
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(scoreFilePath_, ec);
    writeAllowed_ = status.type() == std::filesystem::file_type::not_found &&
                    (!ec || ec == std::errc::no_such_file_or_directory);
}

bool ScoreManager::load() {
    writeAllowed_ = false;
    lastLoadStatus_ = ScoreLoadStatus::IoError;

    auto snapshot = detail::readFileSnapshot(scoreFilePath_);
    if (snapshot.status == detail::FileReadStatus::Missing) {
        completedGames_.clear();
        bestScore_ = 0;
        dirty_ = false;
        migrationPending_ = false;
        sourceSnapshot_.reset();
        writeAllowed_ = true;
        lastLoadStatus_ = ScoreLoadStatus::Missing;
        return true;
    }
    if (snapshot.status != detail::FileReadStatus::Read) {
        return false;
    }

    auto parsed = detail::parseScoreProfile(snapshot.bytes);
    lastLoadStatus_ = parsed.status;
    if (parsed.status != ScoreLoadStatus::LoadedV2 &&
        parsed.status != ScoreLoadStatus::LegacyMigrationPending) {
        return false;
    }
    completedGames_ = std::move(parsed.completedGames);
    bestScore_ = parsed.bestScore;
    migrationPending_ = parsed.status == ScoreLoadStatus::LegacyMigrationPending;
    dirty_ = migrationPending_;
    sourceSnapshot_ = std::move(snapshot.bytes);
    writeAllowed_ = true;
    return true;
}

bool ScoreManager::save() {
    if (!writeAllowed_) {
        lastSaveStatus_ = ScoreSaveStatus::WriteBlocked;
        return false;
    }

    std::string serialized;
    try {
        Json root;
        root["version"] = kFormatVersion;
        root["best_score"] = bestScore_;
        root["completed_games"] = Json::array();
        for (const auto &entry : completedGames_) {
            root["completed_games"].push_back(toJson(entry));
        }
        serialized = root.dump(2) + '\n';
    } catch (const Json::exception &) {
        lastSaveStatus_ = ScoreSaveStatus::SerializationError;
        return false;
    }

    lastSaveStatus_ =
        storage_->save(scoreFilePath_, serialized, sourceSnapshot_, migrationPending_);
    if (lastSaveStatus_ != ScoreSaveStatus::Saved) {
        if (lastSaveStatus_ == ScoreSaveStatus::SourceChanged) {
            // A reload is required to resolve an external change. Ordinary I/O
            // failures remain retryable using the existing in-memory snapshot.
            writeAllowed_ = false;
        }
        return false;
    }
    sourceSnapshot_ = std::move(serialized);
    dirty_ = false;
    migrationPending_ = false;
    return true;
}

ScoreLoadStatus ScoreManager::lastLoadStatus() const noexcept {
    return lastLoadStatus_;
}

ScoreSaveStatus ScoreManager::lastSaveStatus() const noexcept {
    return lastSaveStatus_;
}

bool ScoreManager::needsMigration() const noexcept {
    return migrationPending_;
}

bool ScoreManager::updateBestScore(const int score) noexcept {
    if (score <= bestScore_) {
        return false;
    }
    bestScore_ = score;
    dirty_ = true;
    return true;
}

bool ScoreManager::recordCompletedGame(std::string gameId, const int score, std::string playerName,
                                       std::optional<std::string> playedAtUtc) {
    if (gameId.empty() || score < 0 ||
        std::any_of(completedGames_.begin(), completedGames_.end(),
                    [&gameId](const ScoreEntry &entry) { return entry.gameId == gameId; })) {
        return false;
    }

    ScoreEntry entry;
    entry.score = score;
    if (playerName.empty()) {
        playerName = "Oyuncu";
    }
    entry.playerName = std::move(playerName);
    entry.playedAtUtc = playedAtUtc.value_or(currentUtcIso8601());
    entry.gameId = std::move(gameId);
    completedGames_.push_back(std::move(entry));
    bestScore_ = std::max(bestScore_, score);
    dirty_ = true;
    return true;
}

const std::vector<ScoreEntry> &ScoreManager::completedGames() const noexcept {
    return completedGames_;
}

std::vector<ScoreEntry> ScoreManager::topScores() const {
    auto top = completedGames_;
    std::stable_sort(top.begin(), top.end(), [](const ScoreEntry &lhs, const ScoreEntry &rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        return lhs.playedAtUtc > rhs.playedAtUtc;
    });
    if (top.size() > kTopScoreCount) {
        top.resize(kTopScoreCount);
    }
    return top;
}

int ScoreManager::bestScore() const noexcept {
    return bestScore_;
}

bool ScoreManager::isDirty() const noexcept {
    return dirty_;
}

const std::filesystem::path &ScoreManager::scoreFilePath() const noexcept {
    return scoreFilePath_;
}

std::string ScoreManager::currentUtcIso8601() {
    const std::time_t now = std::time(nullptr);

    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif

    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

} // namespace numtilo_core
