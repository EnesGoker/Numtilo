#include "app/ScoreCoordinator.hpp"

#include "app/GameSession.hpp"

namespace app {

ScoreCoordinator::ScoreCoordinator(numtilo_core::ScoreManager &scoreManager) noexcept
    : scoreManager_(scoreManager) {
}

ScoreCheckpointResult ScoreCoordinator::checkpoint(GameSession &session) {
    if (roundId_ != session.gameId()) {
        roundId_ = session.gameId();
        bestImprovedThisRound_ = false;
    }

    ScoreCheckpointResult result;
    result.bestImproved = scoreManager_.updateBestScore(session.game().getScore());
    result.firstBestOfRound = result.bestImproved && !bestImprovedThisRound_;
    bestImprovedThisRound_ = bestImprovedThisRound_ || result.bestImproved;

    if (session.shouldRecordFinalScore()) {
        scoreManager_.recordCompletedGame(session.gameId(), session.game().getScore(),
                                          session.playerName());
        // Completion is acknowledged in memory even if this ID was already present
        // or the subsequent save fails. Retrying only saves the existing snapshot.
        session.markFinalScoreRecorded();
        result.roundCompleted = true;
    }

    if (!scoreManager_.isDirty() && !scoreManager_.needsMigration()) {
        return result;
    }

    result.saveAttempted = true;
    result.saveSucceeded = scoreManager_.save();
    if (result.saveSucceeded) {
        reportedFailure_.reset();
        return result;
    }

    const auto failure = scoreManager_.lastSaveStatus();
    // SourceChanged permanently blocks ordinary retries until a successful reload.
    // Keep its useful cause instead of issuing a second, generic blocked warning.
    const bool blockedAfterSourceChange =
        failure == numtilo_core::ScoreSaveStatus::WriteBlocked &&
        reportedFailure_ == numtilo_core::ScoreSaveStatus::SourceChanged;
    if (!blockedAfterSourceChange && reportedFailure_ != failure) {
        result.failureToReport = failure;
        reportedFailure_ = failure;
    }
    return result;
}

} // namespace app
