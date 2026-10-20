#include "core/Game.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <utility>

namespace numtilo_core {

namespace {

std::uint32_t nextBounded(std::mt19937 &rng, const std::uint32_t upperExclusive) {
    if (upperExclusive == 0U) {
        return 0U;
    }

    constexpr std::uint64_t range = static_cast<std::uint64_t>(std::mt19937::max()) + 1ULL;
    const std::uint64_t bucketSize = range / upperExclusive;
    const std::uint64_t rejectionLimit = bucketSize * upperExclusive;

    std::uint64_t value = 0;
    do {
        value = static_cast<std::uint64_t>(rng());
    } while (value >= rejectionLimit);

    return static_cast<std::uint32_t>(value / bucketSize);
}

} // namespace

Game::Game() : Game(static_cast<std::uint32_t>(std::random_device{}())) {
}

Game::Game(std::uint32_t seed) {
    reset(seed);
}

void Game::reset() {
    reset(static_cast<std::uint32_t>(std::random_device{}()));
}

void Game::reset(std::uint32_t seed) {
    seed_ = seed;
    rng_.seed(seed_);

    board_.clear();
    state_ = {};
    spawnTile();
    spawnTile();
    updateState();
}

void Game::loadState(const Grid &grid, int score) {
    board_.loadGrid(grid);
    state_ = {};
    state_.score = score;
    updateState();
}

const Game::Grid &Game::getGrid() const noexcept {
    return board_.getGrid();
}

int Game::getScore() const noexcept {
    return state_.score;
}

const GameState &Game::getState() const noexcept {
    return state_;
}

bool Game::hasWon() const noexcept {
    return state_.won;
}

bool Game::isGameOver() const noexcept {
    return state_.gameOver;
}

void Game::updateState() noexcept {
    const auto &grid = board_.getGrid();
    const bool reachedTarget = std::any_of(grid.begin(), grid.end(), [](const auto &row) {
        return std::any_of(row.begin(), row.end(),
                           [](const int value) { return value >= kWinningTileValue; });
    });
    // Achievement belongs to the whole round, even after a 2048 tile merges into 4096.
    state_.won = state_.won || reachedTarget;
    state_.gameOver = !board_.hasAvailableMoves();
}

std::optional<SpawnedTile> Game::spawnTile() {
    const auto emptyCells = board_.emptyCells();

    if (emptyCells.empty()) {
        return std::nullopt;
    }

    const std::size_t cellIndex =
        static_cast<std::size_t>(nextBounded(rng_, static_cast<std::uint32_t>(emptyCells.size())));
    const auto [row, col] = emptyCells[cellIndex];

    const int tileValue = (nextBounded(rng_, 10U) == 0U) ? 4 : 2;

    if (!board_.placeTile(Cell{row, col}, Tile{tileValue})) {
        return std::nullopt;
    }
    return SpawnedTile{row, col, tileValue};
}

MoveResult Game::applyMove(Direction dir, const bool spawnOnMove) {
    auto boardMove = board_.applyMove(dir);
    if (!boardMove.moved) {
        updateState();
        return MoveResult{};
    }

    state_.score += boardMove.scoreDelta;

    MoveResult result;
    result.moved = true;
    result.scoreDelta = boardMove.scoreDelta;
    result.movements = std::move(boardMove.movements);
    result.merges = std::move(boardMove.merges);
    if (spawnOnMove) {
        result.spawnedTile = spawnTile();
    }
    updateState();
    return result;
}

} // namespace numtilo_core
