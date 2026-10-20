#pragma once

#include "app/ui/Scene.hpp"
#include "core/Game.hpp"

#include <string>
#include <string_view>

namespace app::ui {

// Presentation belongs to a round, independently of focus, input and saved scores.
class GameplayOutcome {
  public:
    SceneId nextScene(SceneId current, std::string_view roundId, const numtilo_core::Game &game,
                      bool animationsActive) {
        if (roundId_ != roundId) {
            roundId_ = roundId;
            winPresented_ = false;
        }
        if (current != SceneId::Playing || animationsActive) {
            return current;
        }
        if (game.hasWon() && !winPresented_) {
            winPresented_ = true;
            return SceneId::Win;
        }
        return game.isGameOver() ? SceneId::GameOver : current;
    }

  private:
    std::string roundId_;
    bool winPresented_{false};
};

} // namespace app::ui
