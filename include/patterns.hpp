#pragma once
#include "game.hpp"
namespace gomoku {
struct MoveAssessment {
    int attack = 0, defense = 0, pairs = 0;
    bool legal = false, captureAnchor = false;
};
class MoveAnalyzer {
    const State &state;
    std::array<int8_t, Cells + 1> board;

  public:
    explicit MoveAnalyzer(const State &s);
    MoveAssessment inspect(Move move) const;
};
} // namespace gomoku
