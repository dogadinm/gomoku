#pragma once
#include "session.hpp"
#include <chrono>
#include <memory>
namespace gomoku {
struct SearchResult {
    Move move{};
    Choice choice = Choice::None;
    int depth = 0, score = 0;
    uint64_t nodes = 0, hits = 0;
    double milliseconds = 0;
    FixedList<Move, 64> principal;
    bool lowMemory = false;
};
struct HistoryProfile {
    std::array<std::array<int, 4>, 3> axes{};
    std::array<int, 3> captures{};
};
struct Evaluation {
    std::array<int, 3> alignments{}, potentialCaptures{}, captures{}, figures{}, dynamic{},
        position{};
    int score(int turn) const;
};
class AI {
  public:
    SearchResult search(const State &state, int milliseconds = 420, int maxDepth = 12,
                        const HistoryProfile &profile = {});
    SearchResult act(const Session &session, int milliseconds = 420);
    static int evaluate(const State &s);
    static Evaluation inspect(const State &s, const HistoryProfile &profile = {});
    static HistoryProfile historyProfile(const Session &session);

  private:
    struct Entry {
        uint64_t key = 0;
        int depth = -1, score = 0, flag = 0;
        Move best{};
    };
    struct Ranked {
        Move move;
        int value;
        bool forcing;
        bool changesCaptures;
    };
    static constexpr size_t TableSize = 1 << 16;
    std::unique_ptr<Entry[]> table;
    Move rootBest;
    std::chrono::steady_clock::time_point deadline;
    HistoryProfile history;
    uint64_t nodes = 0, hits = 0;
    int negamax(const State &s, int depth, int alpha, int beta, int ply);
    FixedList<Ranked, Cells> ordered(const State &s, int ply, Move preferred) const;
    void checkTime() const;
};
} // namespace gomoku
