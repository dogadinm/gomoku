#pragma once
#include "rules/game.hpp"
namespace gomoku {
// Shape a stone creates on one axis. Flex: open (two ways on); Block: one way.
enum Shape : uint8_t {
    ShapeNone,
    Block1,
    Flex1,
    Block2,
    Flex2,
    Block3,
    Flex3,
    Block4,
    Flex4,
    Five,
    ShapeCount
};
// One axis of a cell for one player, precomputed for all 65536 neighbor codes.
struct LineInfo {
    uint8_t shape = ShapeNone;
    uint8_t pairs = 0; // opponent pairs captured on this axis
    uint8_t flags = 0;
};
constexpr uint8_t FreeThreeFlag = 1, ThreatFlag = 2; // rule free three; X O O _ capture threat
// Tactical level of an empty cell for one player, combining its four axes.
enum Level : uint8_t {
    LevelNone,
    LevelLow,
    LevelThree,       // makes an open three
    LevelFour,        // makes a four (one completion square)
    LevelDoubleThree, // two open threes
    LevelFourThree,   // a four and an open three
    LevelOpenFour,    // open four or two fours: one block cannot stop it
    LevelFive
};
// The four axes of a cell for one player, updated incrementally.
struct CellSummary {
    uint8_t shape[4], axisPairs[4], axisFlags[4];
    uint8_t pairs, freeThrees, level;
    uint16_t shapes; // the four shapes as one number: index of the level table
    uint8_t flags() const {
        return axisFlags[0] | axisFlags[1] | axisFlags[2] | axisFlags[3];
    }
};
// Search board: walls around the 19x19, make/unmake, incremental codes and summaries.
class Board {
  public:
    static constexpr int Pad = 4, Width = Size + 2 * Pad, Area = Width * Width, Wall = 3;
    static constexpr int Step[4] = {1, Width, Width + 1, Width - 1};
    struct Undo {
        int cell = 0, count = 0, captured = 0, winner = 0, pending = 0;
        int removed[16];
    };
    static int index(Move m) {
        return (m.y + Pad) * Width + m.x + Pad;
    }
    static Move move(int i) {
        return {i % Width - Pad, i / Width - Pad};
    }
    static const int *cells(); // the 361 playable indices
    explicit Board(const State &s);
    int at(int i) const {
        return cell[i];
    }
    bool near(int i) const {
        return around[i] != 0;
    }
    const LineInfo &line(int i, int axis, int player) const;
    const CellSummary &summary(int i, int player) const {
        return summaries[i][player - 1];
    }
    bool legal(int i) const {
        return legalFor(i, turn);
    }
    bool legalFor(int i, int player) const;
    int capturePairs(int i, int player) const;
    bool fiveAt(int i, int player) const;
    bool hasFive(int player) const;
    void make(int i, Undo &undo);
    void unmake(const Undo &undo);
    uint64_t hash() const;
    State state() const;
    int turn = 1, winner = 0, pending = 0, moves = 0, stones = 0;
    std::array<int, 3> captured{};
    Rules rules{};

  private:
    std::array<int8_t, Area> cell{};
    std::array<uint8_t, Area> around{};
    std::array<std::array<uint16_t, 4>, Area> code{};
    std::array<std::array<CellSummary, 2>, Area> summaries{};
    uint64_t key = 0;
    void put(int i, int value);
    void summarize(int i, int axis);
    bool captureDefense(int player);
};
} // namespace gomoku
