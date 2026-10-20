#pragma once

#include "core/Board.hpp"

#include <optional>

namespace app::ui {

enum class SceneId { Splash, HighScores, Playing, Win, GameOver };
enum class SceneCommand {
    None,
    StartGame,
    ShowHighScores,
    ShowSplash,
    RestartGame,
    ToggleSound,
    ContinueGame
};

struct SceneAction {
    SceneCommand command{SceneCommand::None};
    std::optional<numtilo_core::Direction> direction;
};

} // namespace app::ui
