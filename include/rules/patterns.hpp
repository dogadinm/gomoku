#pragma once
// Precomputed knowledge about lines, built once at startup (patterns.cpp):
// the shape a stone creates on an axis for every neighbor code, the level of
// a cell from its four shapes, and the Zobrist keys.
#include "rules/board.hpp"
namespace gomoku {
// Neighbor k of a line code sits at offset k - 4 (k < 4) or k - 3 (k >= 4).
inline int offsetOf(int k) {
    return k < 4 ? k - 4 : k - 3;
}
struct Tables {
    // Per player and neighbor code: shape, capturable pairs, rule flags.
    std::array<std::array<LineInfo, 65536>, 2> info{};
    // Level of a cell indexed by its four shapes as base-ShapeCount digits.
    std::array<uint8_t, ShapeCount * ShapeCount * ShapeCount * ShapeCount> levels{};
    // Zobrist keys: stones per cell, and the rest of the position.
    std::array<std::array<uint64_t, Board::Area>, 3> zobrist{};
    std::array<uint64_t, 3> pending{};
    std::array<std::array<uint64_t, 32>, 3> captured{};
    uint64_t turn = 0, rules[8]{};
    // Padded indices of the 361 playable cells.
    std::array<int, Cells> cells{};
    std::array<std::array<int8_t, 65536>, 2> memo{}; // classify() cache while building
    Tables();
    int classify(unsigned code, int player);
};
const Tables &tables();
} // namespace gomoku
