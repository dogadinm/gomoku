#pragma once
// Shared by table.cpp, moves.cpp, search.cpp, threats.cpp and ai.cpp.
#include "ai/ai.hpp"
#include <algorithm>
#include <chrono>
namespace gomoku {
constexpr int Infinity = 20000000;
constexpr int Exact = 0, Lower = 1, Upper = 2;    // bound stored in the table
constexpr int WinNow = 1, Forced = 2, Defend = 4; // situation reported by generate()
constexpr uint8_t OurWin = 0x40, TheirWin = 0x80; // candidate flags beside LineInfo flags
// What generate() keeps: all relevant moves, or only fours.
enum Mode { Normal, Quiescence, Threats };
struct Timeout {};
inline int terminal(const Board &b, int ply) {
    return b.winner == 3 ? 0 : b.winner == b.turn ? AI::Win - ply : -(AI::Win - ply);
}
// One search thread: own board, killers and history; the table is shared.
struct AI::Worker {
    AI &ai;
    Board board;
    int killers[MaxPly + 1][2];
    int history[3][Board::Area]{};
    uint64_t nodes = 0, hits = 0;
    int selective = 0, rootBest = -1;
    Worker(AI &owner, const State &s) : ai(owner), board(s) {
        for (auto &row : killers)
            row[0] = row[1] = -1;
    }
    void visit(int ply) {
        selective = std::max(selective, ply);
        if ((++nodes & 1023) == 0 && (ai.stop.load(std::memory_order_relaxed) ||
                                      std::chrono::steady_clock::now() >= ai.deadline))
            throw Timeout{};
    }
    int evaluate() const {
        return inspect(board, ai.profile).score(board.turn);
    }
    int generate(Candidates &out, Mode mode, int ttMove, int ply, int depth = 0) const;
    int quiescence(int alpha, int beta, int ply, int qdepth);
    int negamax(int depth, int alpha, int beta, int ply);
    bool vcf(int attacker, int depth, long &budget, int ply);
};
} // namespace gomoku
