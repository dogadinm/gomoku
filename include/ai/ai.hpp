#pragma once
#include "rules/board.hpp"
#include "rules/session.hpp"
#include <atomic>
#include <chrono>
#include <memory>
namespace gomoku {
struct SearchResult {
    Move move{};
    Choice choice = Choice::None;
    // depth: last fully completed iteration; selective: deepest ply reached
    // with quiescence; threat: a forced win by fours was proven.
    int depth = 0, score = 0, selective = 0, threat = 0, threads = 1;
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
    // A score at or beyond WinBound is a proven result: Win - plies to the end.
    static constexpr int Win = 10000000, WinBound = Win - 1000, MinimumDepth = 10;
    SearchResult search(const State &state, int milliseconds = 420, int maxDepth = 32,
                        const HistoryProfile &profile = {});
    SearchResult act(const Session &session, int milliseconds = 420);
    static int evaluate(const State &s);
    static Evaluation inspect(const State &s, const HistoryProfile &profile = {});
    static Evaluation inspect(const Board &b, const HistoryProfile &profile = {});
    static HistoryProfile historyProfile(const Session &session);

  private:
    struct Worker;
    struct Candidate {
        int16_t cell;
        uint8_t ours, theirs; // tactical level of the cell for each side
        uint8_t pairs, flags;
        int order;
    };
    using Candidates = FixedList<Candidate, Cells>;
    struct Entry {
        int score = 0, move = -1, depth = -1, flag = 0, generation = 0;
    };
    static constexpr int MaxPly = 64;
    // Transposition table shared by all search threads without locks: each
    // entry is two words, the key stored XOR-ed with the data, so a torn
    // write from a concurrent thread simply fails the key check.
    std::unique_ptr<std::atomic<uint64_t>[]> table;
    size_t entries = 0;
    int generation = 0;
    HistoryProfile profile;
    std::chrono::steady_clock::time_point deadline;
    std::atomic<bool> stop{false};
    bool probe(uint64_t key, Entry &entry) const;
    void store(uint64_t key, int score, int move, int depth, int flag);
    static const Candidate &pick(Candidates &moves, size_t n);
};
} // namespace gomoku
