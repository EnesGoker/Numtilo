#include "core/Board.hpp"

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
using Row = std::array<int, Board::kGridSize>;

constexpr Board::Grid makeGrid(Row first, Row second, Row third, Row fourth) {
    return {first, second, third, fourth};
}

constexpr std::array<Direction, 4> kDirections{Direction::Left, Direction::Right, Direction::Up,
                                               Direction::Down};

constexpr std::string_view directionName(Direction direction) {
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

struct MoveExpectation {
    Direction direction;
    Board::Grid grid;
    int scoreDelta;
};

struct BoardFixture {
    std::string_view name;
    Board::Grid initial;
    std::array<MoveExpectation, 4> expectations;
};

enum class Transform { HorizontalMirror, VerticalMirror, Transpose };

constexpr std::array<Transform, 3> kTransforms{Transform::HorizontalMirror,
                                               Transform::VerticalMirror, Transform::Transpose};

constexpr std::string_view transformName(Transform transform) {
    switch (transform) {
    case Transform::HorizontalMirror:
        return "horizontal mirror";
    case Transform::VerticalMirror:
        return "vertical mirror";
    case Transform::Transpose:
        return "transpose";
    }
    return "unknown";
}

constexpr Cell transformCell(Cell cell, Transform transform) {
    switch (transform) {
    case Transform::HorizontalMirror:
        return {cell.row, Board::kGridSize - 1 - cell.col};
    case Transform::VerticalMirror:
        return {Board::kGridSize - 1 - cell.row, cell.col};
    case Transform::Transpose:
        return {cell.col, cell.row};
    }
    return cell;
}

Board::Grid transformGrid(const Board::Grid &grid, Transform transform) {
    Board::Grid transformed{};
    for (int row = 0; row < Board::kGridSize; ++row) {
        for (int col = 0; col < Board::kGridSize; ++col) {
            const Cell destination = transformCell({row, col}, transform);
            transformed[destination.row][destination.col] = grid[row][col];
        }
    }
    return transformed;
}

// Transform the direction along with the board, so both moves have the same spatial meaning.
constexpr Direction transformDirection(Direction direction, Transform transform) {
    switch (transform) {
    case Transform::HorizontalMirror:
        if (direction == Direction::Left) {
            return Direction::Right;
        }
        if (direction == Direction::Right) {
            return Direction::Left;
        }
        return direction;
    case Transform::VerticalMirror:
        if (direction == Direction::Up) {
            return Direction::Down;
        }
        if (direction == Direction::Down) {
            return Direction::Up;
        }
        return direction;
    case Transform::Transpose:
        switch (direction) {
        case Direction::Left:
            return Direction::Up;
        case Direction::Right:
            return Direction::Down;
        case Direction::Up:
            return Direction::Left;
        case Direction::Down:
            return Direction::Right;
        }
    }
    return direction;
}

using MovementRecord = std::tuple<int, int, int, int, int, bool>;
using MergeRecord = std::tuple<int, int, int>;

std::vector<MovementRecord> movementRecords(const BoardMoveResult &result) {
    std::vector<MovementRecord> records;
    records.reserve(result.movements.size());
    for (const auto &movement : result.movements) {
        records.emplace_back(movement.value, movement.from.row, movement.from.col, movement.to.row,
                             movement.to.col, movement.partOfMerge);
    }
    std::sort(records.begin(), records.end());
    return records;
}

std::vector<MergeRecord> mergeRecords(const BoardMoveResult &result) {
    std::vector<MergeRecord> records;
    records.reserve(result.merges.size());
    for (const auto &merge : result.merges) {
        records.emplace_back(merge.value, merge.cell.row, merge.cell.col);
    }
    std::sort(records.begin(), records.end());
    return records;
}

BoardMoveResult transformResult(BoardMoveResult result, Transform transform) {
    for (auto &movement : result.movements) {
        movement.from = transformCell(movement.from, transform);
        movement.to = transformCell(movement.to, transform);
    }
    for (auto &merge : result.merges) {
        merge.cell = transformCell(merge.cell, transform);
    }
    return result;
}

Board::Grid randomValidGrid(std::mt19937 &generator) {
    constexpr std::array<int, 12> values{0, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048};
    Board::Grid grid{};
    for (auto &row : grid) {
        for (int &value : row) {
            // Direct engine output keeps this corpus stable across standard library distributions.
            value = values[generator() % values.size()];
        }
    }
    return grid;
}

} // namespace

TEST_CASE("asymmetric full boards have explicit outcomes in every direction",
          "[board][move][full-board]") {
    // Expected grids and scores are hand-calculated, rather than generated by another move routine.
    constexpr std::array<BoardFixture, 2> fixtures{{
        {"row merges with gaps and unequal columns",
         makeGrid({2, 0, 2, 4}, {4, 4, 8, 0}, {0, 2, 2, 2}, {8, 0, 8, 8}),
         {{
             {Direction::Left, makeGrid({4, 4, 0, 0}, {8, 8, 0, 0}, {4, 2, 0, 0}, {16, 8, 0, 0}),
              32},
             {Direction::Right, makeGrid({0, 0, 4, 4}, {0, 0, 8, 8}, {0, 0, 2, 4}, {0, 0, 8, 16}),
              32},
             {Direction::Up, makeGrid({2, 4, 2, 4}, {4, 2, 8, 2}, {8, 0, 2, 8}, {0, 0, 8, 0}), 0},
             {Direction::Down, makeGrid({0, 0, 2, 0}, {2, 0, 8, 4}, {4, 4, 2, 2}, {8, 2, 8, 8}), 0},
         }}},
        {"column pairs and triples stop after one merge",
         makeGrid({2, 4, 0, 8}, {2, 0, 4, 8}, {4, 4, 4, 0}, {0, 4, 8, 8}),
         {{
             {Direction::Left, makeGrid({2, 4, 8, 0}, {2, 4, 8, 0}, {8, 4, 0, 0}, {4, 16, 0, 0}),
              24},
             {Direction::Right, makeGrid({0, 2, 4, 8}, {0, 2, 4, 8}, {0, 0, 4, 8}, {0, 0, 4, 16}),
              24},
             {Direction::Up, makeGrid({4, 8, 8, 16}, {4, 4, 8, 8}, {0, 0, 0, 0}, {0, 0, 0, 0}), 36},
             {Direction::Down, makeGrid({0, 0, 0, 0}, {0, 0, 0, 0}, {4, 4, 8, 8}, {4, 8, 8, 16}),
              36},
         }}},
    }};

    for (const auto &fixture : fixtures) {
        for (const auto &expectation : fixture.expectations) {
            INFO("Fixture: " << fixture.name);
            INFO("Direction: " << directionName(expectation.direction));
            Board board(fixture.initial);
            const auto result = board.applyMove(expectation.direction);

            REQUIRE(board.getGrid() == expectation.grid);
            REQUIRE(result.moved);
            REQUIRE(result.scoreDelta == expectation.scoreDelta);
        }
    }
}

TEST_CASE("moves and animation records respect mirrors and transposition",
          "[board][move][property][move-trace]") {
    constexpr std::uint32_t seed = 0x2048B04DU;
    constexpr int boardCount = 200;
    std::mt19937 generator(seed);

    for (int sample = 0; sample < boardCount; ++sample) {
        const auto initial = randomValidGrid(generator);
        for (const Direction direction : kDirections) {
            for (const Transform transform : kTransforms) {
                INFO("Seed: " << seed << ", board: " << sample);
                INFO("Direction: " << directionName(direction));
                INFO("Transform: " << transformName(transform));

                // Both boards start independently; later directions never reuse an earlier result.
                Board original(initial);
                Board transformed(transformGrid(initial, transform));
                const auto originalResult = original.applyMove(direction);
                const auto transformedResult =
                    transformed.applyMove(transformDirection(direction, transform));
                const auto expectedResult = transformResult(originalResult, transform);

                REQUIRE(transformed.getGrid() == transformGrid(original.getGrid(), transform));
                REQUIRE(transformedResult.moved == expectedResult.moved);
                REQUIRE(transformedResult.scoreDelta == expectedResult.scoreDelta);
                // A reflection can reverse line traversal order. Compare records as multisets while
                // retaining every coordinate, value, merge flag and duplicate record.
                REQUIRE(movementRecords(transformedResult) == movementRecords(expectedResult));
                REQUIRE(mergeRecords(transformedResult) == mergeRecords(expectedResult));
            }
        }
    }
}
