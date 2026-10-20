#pragma once

#include "core/Board.hpp"
#include "core/GameState.hpp"

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace numtilo_core {

struct SpawnedTile {
    int row;
    int col;
    int value;
};

struct MoveResult {
    bool moved{false};
    int scoreDelta{0};
    std::optional<SpawnedTile> spawnedTile;
    std::vector<TileMovement> movements;
    std::vector<TileMerge> merges;
};

// Coordinates turns, spawning, score, and independent win/game-over flags; the app owns the UI.
class Game {
  public:
    static constexpr int kGridSize = Board::kGridSize;
    static constexpr int kWinningTileValue = 2048;
    using Grid = Board::Grid;

    Game();
    explicit Game(std::uint32_t seed);

    void reset();
    void reset(std::uint32_t seed);
    // Replaces grid, score and flags from the loaded board, without reseeding the RNG.
    void loadState(const Grid &grid, int score = 0);

    MoveResult applyMove(Direction dir, bool spawnOnMove = true);

    const Grid &getGrid() const noexcept;
    int getScore() const noexcept;
    const GameState &getState() const noexcept;
    bool hasWon() const noexcept;
    bool isGameOver() const noexcept;

  private:
    std::optional<SpawnedTile> spawnTile();
    void updateState() noexcept;

    Board board_;
    GameState state_;
    std::mt19937 rng_;
    std::uint32_t seed_{0};
};

} // namespace numtilo_core
