#include "app/GameSession.hpp"

#include <atomic>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace app {

namespace {

std::string nextGameId() {
    // This generator never touches Game's RNG, so identity does not alter tile spawning.
    static const std::string processNonce = [] {
        std::random_device entropy;
        std::uniform_int_distribution<std::uint32_t> word;
        std::ostringstream output;
        output << std::hex << std::setfill('0');
        for (int index = 0; index < 4; ++index) {
            output << std::setw(8) << word(entropy);
        }
        return output.str();
    }();
    static std::atomic<std::uint64_t> counter{0};

    auto previous = counter.load(std::memory_order_relaxed);
    do {
        if (previous == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error("Game session identity counter exhausted");
        }
    } while (!counter.compare_exchange_weak(previous, previous + 1, std::memory_order_relaxed));

    return processNonce + "-" + std::to_string(previous + 1);
}

} // namespace

GameSession::GameSession() : gameId_(nextGameId()) {
}

GameSession::GameSession(const std::uint32_t seed) : game_(seed), gameId_(nextGameId()) {
}

void GameSession::setPlayerName(std::string playerName) {
    playerName_ = playerName.empty() ? "Oyuncu" : std::move(playerName);
}

const std::string &GameSession::playerName() const noexcept {
    return playerName_;
}

const std::string &GameSession::gameId() const noexcept {
    return gameId_;
}

void GameSession::resetGame() {
    auto nextId = nextGameId();
    game_.reset();
    gameId_ = std::move(nextId);
    finalScoreRecorded_ = false;
}

void GameSession::resetGame(const std::uint32_t seed) {
    auto nextId = nextGameId();
    game_.reset(seed);
    gameId_ = std::move(nextId);
    finalScoreRecorded_ = false;
}

numtilo_core::MoveResult GameSession::applyMove(const numtilo_core::Direction direction) {
    return game_.applyMove(direction);
}

const numtilo_core::Game &GameSession::game() const noexcept {
    return game_;
}

bool GameSession::shouldRecordFinalScore() const noexcept {
    return game_.isGameOver() && !finalScoreRecorded_;
}

void GameSession::markFinalScoreRecorded() noexcept {
    if (game_.isGameOver()) {
        finalScoreRecorded_ = true;
    }
}

} // namespace app
