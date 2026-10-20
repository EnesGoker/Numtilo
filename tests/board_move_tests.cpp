#include "core/Board.hpp"
#include "core/Game.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

using numtilo_core::Board;
using numtilo_core::BoardMoveResult;
using numtilo_core::Cell;
using numtilo_core::Direction;
using numtilo_core::Game;
using numtilo_core::TileMerge;
using numtilo_core::TileMovement;
using Line = std::array<int, Board::kGridSize>;

constexpr std::array kDirections{Direction::Left, Direction::Right, Direction::Up, Direction::Down};

std::string_view directionName(const Direction direction) {
    switch (direction) {
    case Direction::Left:
        return "left";
    case Direction::Right:
        return "right";
    case Direction::Up:
        return "up";
    case Direction::Down:
        return "down";
    }
    return "unknown";
}

bool isForward(const Direction direction) {
    return direction == Direction::Left || direction == Direction::Up;
}

// Index always follows physical coordinates, regardless of the move direction.
// Right/down expectations are specified explicitly rather than reversing left/up results.
Cell lineCell(const Direction direction, const int line, const int index) {
    if (direction == Direction::Left || direction == Direction::Right) {
        return {line, index};
    }
    return {index, line};
}

Board::Grid gridWithLine(const Line &values, const Direction direction, const int line) {
    Board::Grid grid{};
    for (int index = 0; index < Board::kGridSize; ++index) {
        const Cell cell = lineCell(direction, line, index);
        grid[cell.row][cell.col] = values[index];
    }
    return grid;
}

using MovementRecord = std::tuple<int, int, int, int, int, bool>;
using MergeRecord = std::tuple<int, int, int>;

std::vector<MovementRecord> movementRecords(const std::vector<TileMovement> &movements) {
    std::vector<MovementRecord> records;
    for (const auto &movement : movements) {
        records.emplace_back(movement.value, movement.from.row, movement.from.col, movement.to.row,
                             movement.to.col, movement.partOfMerge);
    }
    std::sort(records.begin(), records.end());
    return records;
}

std::vector<MergeRecord> mergeRecords(const std::vector<TileMerge> &merges) {
    std::vector<MergeRecord> records;
    for (const auto &merge : merges) {
        records.emplace_back(merge.cell.row, merge.cell.col, merge.value);
    }
    std::sort(records.begin(), records.end());
    return records;
}

bool isInBounds(const Cell cell) {
    return cell.row >= 0 && cell.row < Board::kGridSize && cell.col >= 0 &&
           cell.col < Board::kGridSize;
}

// Check the observable contract without implementing a second merge algorithm.
void requireMoveContract(const Board::Grid &before, const Board::Grid &after,
                         const BoardMoveResult &result, const Direction direction) {
    REQUIRE(result.moved == (before != after));

    int sumBefore = 0;
    int sumAfter = 0;
    int countBefore = 0;
    int countAfter = 0;
    for (int row = 0; row < Board::kGridSize; ++row) {
        for (int col = 0; col < Board::kGridSize; ++col) {
            sumBefore += before[row][col];
            sumAfter += after[row][col];
            countBefore += before[row][col] != 0;
            countAfter += after[row][col] != 0;
        }
    }
    REQUIRE(sumAfter == sumBefore);
    REQUIRE(countBefore - countAfter == static_cast<int>(result.merges.size()));

    for (int line = 0; line < Board::kGridSize; ++line) {
        bool encounteredEmpty = false;
        for (int offset = 0; offset < Board::kGridSize; ++offset) {
            const int index = isForward(direction) ? offset : Board::kGridSize - 1 - offset;
            const Cell cell = lineCell(direction, line, index);
            if (after[cell.row][cell.col] == 0) {
                encounteredEmpty = true;
            } else {
                // No occupied cell may appear beyond an empty cell when viewed from the target
                // edge.
                REQUIRE_FALSE(encounteredEmpty);
            }
        }
    }

    Board::Grid sourceCounts{};
    Board::Grid mergeSourceCounts{};
    Board::Grid slideCounts{};
    Board::Grid mergeValues{};
    Board::Grid reconstructed = before;
    for (const auto &movement : result.movements) {
        CAPTURE(movement.value, movement.from.row, movement.from.col, movement.to.row,
                movement.to.col, movement.partOfMerge);
        REQUIRE(isInBounds(movement.from));
        REQUIRE(isInBounds(movement.to));
        REQUIRE(movement.value > 0);
        REQUIRE(before[movement.from.row][movement.from.col] == movement.value);
        REQUIRE(++sourceCounts[movement.from.row][movement.from.col] == 1);
        reconstructed[movement.from.row][movement.from.col] = 0;

        if (direction == Direction::Left || direction == Direction::Right) {
            REQUIRE(movement.from.row == movement.to.row);
            if (direction == Direction::Left) {
                REQUIRE(movement.to.col <= movement.from.col);
            } else {
                REQUIRE(movement.to.col >= movement.from.col);
            }
        } else {
            REQUIRE(movement.from.col == movement.to.col);
            if (direction == Direction::Up) {
                REQUIRE(movement.to.row <= movement.from.row);
            } else {
                REQUIRE(movement.to.row >= movement.from.row);
            }
        }

        if (movement.partOfMerge) {
            REQUIRE(after[movement.to.row][movement.to.col] == 2 * movement.value);
            ++mergeSourceCounts[movement.to.row][movement.to.col];
        } else {
            REQUIRE(movement.from != movement.to);
            REQUIRE(after[movement.to.row][movement.to.col] == movement.value);
            REQUIRE(++slideCounts[movement.to.row][movement.to.col] == 1);
        }
    }

    int scoreFromMerges = 0;
    for (const auto &merge : result.merges) {
        CAPTURE(merge.cell.row, merge.cell.col, merge.value);
        REQUIRE(isInBounds(merge.cell));
        REQUIRE(mergeValues[merge.cell.row][merge.cell.col] == 0);
        REQUIRE(merge.value > 0);
        REQUIRE(after[merge.cell.row][merge.cell.col] == merge.value);
        REQUIRE(mergeSourceCounts[merge.cell.row][merge.cell.col] == 2);
        REQUIRE(slideCounts[merge.cell.row][merge.cell.col] == 0);
        mergeValues[merge.cell.row][merge.cell.col] = merge.value;
        scoreFromMerges += merge.value;
    }
    REQUIRE(result.scoreDelta == scoreFromMerges);

    for (int row = 0; row < Board::kGridSize; ++row) {
        for (int col = 0; col < Board::kGridSize; ++col) {
            REQUIRE(mergeSourceCounts[row][col] == (mergeValues[row][col] != 0 ? 2 : 0));
        }
    }
    for (const auto &movement : result.movements) {
        if (!movement.partOfMerge) {
            REQUIRE(reconstructed[movement.to.row][movement.to.col] == 0);
            reconstructed[movement.to.row][movement.to.col] = movement.value;
        }
    }
    for (const auto &merge : result.merges) {
        REQUIRE(reconstructed[merge.cell.row][merge.cell.col] == 0);
        reconstructed[merge.cell.row][merge.cell.col] = merge.value;
    }
    REQUIRE(reconstructed == after);
}

struct LineFixture {
    std::string_view name;
    Line initial;
    Line forward;
    Line backward;
    int scoreDelta;
};

constexpr std::array kLineFixtures{
    LineFixture{"empty", {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, 0},
    LineFixture{"first edge", {2, 0, 0, 0}, {2, 0, 0, 0}, {0, 0, 0, 2}, 0},
    LineFixture{"middle tile", {0, 2, 0, 0}, {2, 0, 0, 0}, {0, 0, 0, 2}, 0},
    LineFixture{"last edge", {0, 0, 0, 2}, {2, 0, 0, 0}, {0, 0, 0, 2}, 0},
    LineFixture{"packed distinct", {2, 4, 8, 16}, {2, 4, 8, 16}, {2, 4, 8, 16}, 0},
    LineFixture{"sparse distinct", {0, 2, 0, 4}, {2, 4, 0, 0}, {0, 0, 2, 4}, 0},
    LineFixture{"alternating gaps", {2, 0, 4, 0}, {2, 4, 0, 0}, {0, 0, 2, 4}, 0},
    LineFixture{"adjacent pair", {2, 2, 0, 0}, {4, 0, 0, 0}, {0, 0, 0, 4}, 4},
    LineFixture{"gapped pair", {2, 0, 2, 0}, {4, 0, 0, 0}, {0, 0, 0, 4}, 4},
    LineFixture{"opposite edges", {2, 0, 0, 2}, {4, 0, 0, 0}, {0, 0, 0, 4}, 4},
    LineFixture{"new tile cannot merge again", {2, 2, 4, 0}, {4, 4, 0, 0}, {0, 0, 4, 4}, 4},
    LineFixture{"four equal tiles", {2, 2, 2, 2}, {4, 4, 0, 0}, {0, 0, 4, 4}, 8},
    LineFixture{"leading triple", {2, 2, 2, 0}, {4, 2, 0, 0}, {0, 0, 2, 4}, 4},
    LineFixture{"trailing triple", {0, 2, 2, 2}, {4, 2, 0, 0}, {0, 0, 2, 4}, 4},
    LineFixture{"gapped triple", {2, 0, 2, 2}, {4, 2, 0, 0}, {0, 0, 2, 4}, 4},
    LineFixture{"two different pairs", {2, 2, 4, 4}, {4, 8, 0, 0}, {0, 0, 4, 8}, 12},
    LineFixture{"two descending pairs", {4, 4, 2, 2}, {8, 4, 0, 0}, {0, 0, 8, 4}, 12},
    LineFixture{"higher tile cannot merge again", {4, 4, 8, 0}, {8, 8, 0, 0}, {0, 0, 8, 8}, 8},
    LineFixture{"inner pair", {2, 4, 4, 2}, {2, 8, 2, 0}, {0, 2, 8, 2}, 8},
    LineFixture{"merge between equal neighbours", {4, 2, 2, 4}, {4, 4, 4, 0}, {0, 4, 4, 4}, 4},
    LineFixture{"alternating unequal", {2, 4, 2, 4}, {2, 4, 2, 4}, {2, 4, 2, 4}, 0},
    LineFixture{"2048 merge", {1024, 0, 1024, 0}, {2048, 0, 0, 0}, {0, 0, 0, 2048}, 2048},
    LineFixture{"values beyond 2048",
                {4096, 4096, 2048, 2048},
                {8192, 4096, 0, 0},
                {0, 0, 8192, 4096},
                12288},
    LineFixture{"two ascending pairs", {4, 4, 8, 8}, {8, 16, 0, 0}, {0, 0, 8, 16}, 24},
};

struct LineMovement {
    int value;
    int from;
    int to;
    bool partOfMerge;
};

struct LineMerge {
    int index;
    int value;
};

struct TraceFixture {
    std::string_view name;
    Line initial;
    std::vector<LineMovement> forwardMovements;
    std::vector<LineMovement> backwardMovements;
    std::vector<LineMerge> forwardMerges;
    std::vector<LineMerge> backwardMerges;
};

const std::array kTraceFixtures{
    TraceFixture{"slide with stationary tile",
                 {0, 2, 0, 4},
                 {{2, 1, 0, false}, {4, 3, 1, false}},
                 {{2, 1, 2, false}},
                 {},
                 {}},
    TraceFixture{"stationary merge participant",
                 {2, 2, 0, 8},
                 {{2, 0, 0, true}, {2, 1, 0, true}, {8, 3, 1, false}},
                 {{2, 0, 2, true}, {2, 1, 2, true}},
                 {{0, 4}},
                 {{2, 4}}},
    TraceFixture{"triple respects leading edge",
                 {2, 2, 2, 0},
                 {{2, 0, 0, true}, {2, 1, 0, true}, {2, 2, 1, false}},
                 {{2, 1, 3, true}, {2, 2, 3, true}, {2, 0, 2, false}},
                 {{0, 4}},
                 {{3, 4}}},
    TraceFixture{"two independent merges",
                 {2, 2, 2, 2},
                 {{2, 0, 0, true}, {2, 1, 0, true}, {2, 2, 1, true}, {2, 3, 1, true}},
                 {{2, 0, 2, true}, {2, 1, 2, true}, {2, 2, 3, true}, {2, 3, 3, true}},
                 {{0, 4}, {1, 4}},
                 {{2, 4}, {3, 4}}},
    TraceFixture{"no repeated merge",
                 {2, 2, 4, 0},
                 {{2, 0, 0, true}, {2, 1, 0, true}, {4, 2, 1, false}},
                 {{2, 0, 2, true}, {2, 1, 2, true}, {4, 2, 3, false}},
                 {{0, 4}},
                 {{2, 4}}},
    TraceFixture{"inner pair with stationary edge",
                 {2, 4, 4, 2},
                 {{4, 1, 1, true}, {4, 2, 1, true}, {2, 3, 2, false}},
                 {{4, 1, 2, true}, {4, 2, 2, true}, {2, 0, 1, false}},
                 {{1, 8}},
                 {{2, 8}}},
};

std::vector<TileMovement> expectedMovements(const std::vector<LineMovement> &movements,
                                            const Direction direction, const int line) {
    std::vector<TileMovement> expected;
    for (const auto &movement : movements) {
        expected.push_back({movement.value, lineCell(direction, line, movement.from),
                            lineCell(direction, line, movement.to), movement.partOfMerge});
    }
    return expected;
}

std::vector<TileMerge> expectedMerges(const std::vector<LineMerge> &merges,
                                      const Direction direction, const int line) {
    std::vector<TileMerge> expected;
    for (const auto &merge : merges) {
        expected.push_back({lineCell(direction, line, merge.index), merge.value});
    }
    return expected;
}

} // namespace

TEST_CASE("Moves obey explicit line rules in every direction and position",
          "[board][game][move][move-matrix]") {
    constexpr int initialScore = 37;
    for (const auto &fixture : kLineFixtures) {
        for (const Direction direction : kDirections) {
            for (int line = 0; line < Board::kGridSize; ++line) {
                DYNAMIC_SECTION(fixture.name << " / " << directionName(direction) << " / line "
                                             << line) {
                    const auto initial = gridWithLine(fixture.initial, direction, line);
                    const auto expected = gridWithLine(
                        isForward(direction) ? fixture.forward : fixture.backward, direction, line);
                    Board board(initial);
                    const auto result = board.applyMove(direction);
                    REQUIRE(board.getGrid() == expected);
                    REQUIRE(result.scoreDelta == fixture.scoreDelta);
                    requireMoveContract(initial, board.getGrid(), result, direction);

                    Game game(0x2048U);
                    game.loadState(initial, initialScore);
                    const auto gameResult = game.applyMove(direction, false);
                    REQUIRE(game.getGrid() == expected);
                    REQUIRE(gameResult.moved == (initial != expected));
                    REQUIRE(gameResult.scoreDelta == fixture.scoreDelta);
                    REQUIRE_FALSE(gameResult.spawnedTile.has_value());
                    REQUIRE(game.getScore() == initialScore + fixture.scoreDelta);
                    REQUIRE(game.getState().score == game.getScore());
                    REQUIRE(movementRecords(gameResult.movements) ==
                            movementRecords(result.movements));
                    REQUIRE(mergeRecords(gameResult.merges) == mergeRecords(result.merges));
                }
            }
        }
    }
}

TEST_CASE("Move records identify the exact original tiles and merge targets",
          "[board][move][move-trace]") {
    for (const auto &fixture : kTraceFixtures) {
        for (const Direction direction : kDirections) {
            for (int line = 0; line < Board::kGridSize; ++line) {
                DYNAMIC_SECTION(fixture.name << " / " << directionName(direction) << " / line "
                                             << line) {
                    Board board(gridWithLine(fixture.initial, direction, line));
                    const auto result = board.applyMove(direction);
                    const auto movements = expectedMovements(
                        isForward(direction) ? fixture.forwardMovements : fixture.backwardMovements,
                        direction, line);
                    const auto merges = expectedMerges(
                        isForward(direction) ? fixture.forwardMerges : fixture.backwardMerges,
                        direction, line);
                    REQUIRE(movementRecords(result.movements) == movementRecords(movements));
                    REQUIRE(mergeRecords(result.merges) == mergeRecords(merges));
                }
            }
        }
    }
}

TEST_CASE("A merged tile may merge on the next turn but never twice on the same turn",
          "[board][game][move][move-matrix]") {
    for (const Direction direction : kDirections) {
        for (int line = 0; line < Board::kGridSize; ++line) {
            DYNAMIC_SECTION(directionName(direction) << " / line " << line) {
                Game game(0x2048U);
                game.loadState(gridWithLine({2, 2, 2, 2}, direction, line), 37);
                const auto first = game.applyMove(direction, false);
                REQUIRE(game.getGrid() ==
                        gridWithLine(isForward(direction) ? Line{4, 4, 0, 0} : Line{0, 0, 4, 4},
                                     direction, line));
                REQUIRE(first.moved);
                REQUIRE(first.scoreDelta == 8);
                REQUIRE(first.merges.size() == 2);
                REQUIRE(game.getScore() == 45);
                REQUIRE_FALSE(first.spawnedTile.has_value());

                const auto second = game.applyMove(direction, false);
                REQUIRE(game.getGrid() ==
                        gridWithLine(isForward(direction) ? Line{8, 0, 0, 0} : Line{0, 0, 0, 8},
                                     direction, line));
                REQUIRE(second.moved);
                REQUIRE(second.scoreDelta == 8);
                REQUIRE(second.merges.size() == 1);
                REQUIRE(game.getScore() == 53);
                REQUIRE(game.getState().score == 53);
                REQUIRE_FALSE(second.spawnedTile.has_value());
            }
        }
    }
}

TEST_CASE("Generated full boards preserve values and provide complete move records",
          "[board][move][property][move-trace]") {
    constexpr std::uint32_t seed = 0x2048C04DU;
    constexpr std::array values{0, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048};
    std::mt19937 engine(seed);
    for (int sample = 0; sample < 200; ++sample) {
        Board::Grid initial{};
        for (auto &row : initial) {
            for (int &value : row) {
                value = values[engine() % values.size()];
            }
        }
        for (const Direction direction : kDirections) {
            INFO("seed=" << seed << " sample=" << sample
                         << " direction=" << directionName(direction));
            Board board(initial);
            const auto result = board.applyMove(direction);
            requireMoveContract(initial, board.getGrid(), result, direction);
        }
    }
}
