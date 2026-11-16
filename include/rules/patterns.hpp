#pragma once
// Tables built once: shapes per line code, cell levels, Zobrist keys.
#include "rules/board.hpp"
namespace gomoku {
// Neighbor k of a line code sits at offset k - 4 (k < 4) or k - 3 (k >= 4).
inline int offsetOf(int k) {
    return k < 4 ? k - 4 : k - 3;
}
struct Tables {
    // [player][code]: shape, pairs, flags
    std::array<std::array<LineInfo, 65536>, 2> info{};
    // cell level by its four shapes
    std::array<uint8_t, ShapeCount * ShapeCount * ShapeCount * ShapeCount> levels{};
    // Zobrist keys
    std::array<std::array<uint64_t, Board::Area>, 3> zobrist{};
    std::array<uint64_t, 3> pending{};
    std::array<std::array<uint64_t, 32>, 3> captured{};
    uint64_t turn = 0, rules[8]{};
    // padded indices of the 361 cells
    std::array<int, Cells> cells{};
    std::array<std::array<int8_t, 65536>, 2> memo{}; // classify() cache while building
    Tables();
    int classify(unsigned code, int player);
};
// Built on first use.
inline const Tables &tables() {
    static const Tables value;
    return value;
}
} // namespace gomoku
