#pragma once
#include "fixed_list.hpp"
#include <array>
#include <cstdint>
namespace gomoku {
constexpr int Size = 19, Cells = Size * Size;
struct Move {
    int x = -1, y = -1;
    bool operator==(Move b) const {
        return x == b.x && y == b.y;
    }
};
using MoveList = FixedList<Move, Cells>;
enum class Opening { Standard, Pro, Swap, Swap2 };
const char *openingName(Opening opening);
struct Rules {
    bool captures = true, doubleThree = true;
    Opening opening = Opening::Standard;
};
struct State {
    std::array<int8_t, Cells> board{};
    std::array<int, 3> captured{};
    int turn = 1, winner = 0, pending = 0, moves = 0;
    Rules rules{};
    int at(int x, int y) const {
        return x < 0 || y < 0 || x >= Size || y >= Size ? -1 : board[y * Size + x];
    }
    bool hasFive(int player) const;
    bool legal(Move m) const;
    bool play(Move m);
    MoveList candidates(bool legalOnly = true) const;
    uint64_t hash() const;

  private:
    friend class AI;
    void applyLegal(Move m);
    bool fiveAt(Move m, int player) const;
    int place(Move m);
    bool doubleThree(Move m) const;
    bool captureDefense(int player) const;
};
} // namespace gomoku
