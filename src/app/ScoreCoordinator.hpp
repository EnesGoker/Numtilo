#pragma once

#include "core/ScoreManager.hpp"

#include <optional>
#include <string>

namespace app {

class GameSession;

struct ScoreCheckpointResult {
    bool bestImproved{false};
    bool firstBestOfRound{false};
    bool roundCompleted{false};
    bool saveAttempted{false};
    bool saveSucceeded{false};
    std::optional<numtilo_core::ScoreSaveStatus> failureToReport;
};

// Coordinates one active session's sequential rounds with a profile. The referenced
// ScoreManager must outlive this coordinator; screens and sounds consume the result.
class ScoreCoordinator {
  public:
    explicit ScoreCoordinator(numtilo_core::ScoreManager &scoreManager) noexcept;

    ScoreCoordinator(const ScoreCoordinator &) = delete;
    ScoreCoordinator &operator=(const ScoreCoordinator &) = delete;
    ScoreCoordinator(ScoreCoordinator &&) = delete;
    ScoreCoordinator &operator=(ScoreCoordinator &&) = delete;

    // Call at meaningful boundaries: changed moves, startup, and before leaving or
    // resetting a round. Failed snapshots retry here, never on an idle-frame timer.
    ScoreCheckpointResult checkpoint(GameSession &session);

  private:
    numtilo_core::ScoreManager &scoreManager_;
    std::string roundId_;
    bool bestImprovedThisRound_{false};
    std::optional<numtilo_core::ScoreSaveStatus> reportedFailure_;
};

} // namespace app
