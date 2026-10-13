#pragma once
#include "game.hpp"
#include <chrono>
#include <vector>
namespace gomoku {
struct SearchResult {
 Move move{}; int depth=0,score=0; uint64_t nodes=0,hits=0; double milliseconds=0;
 std::vector<Move> principal;
};
class AI {
public:
 SearchResult search(const State &state,int milliseconds=420,int maxDepth=12);
 static int evaluate(const State &s);
private:
 struct Entry {uint64_t key=0; int depth=-1,score=0,flag=0; Move best{};};
 struct Ranked {Move move; int value; bool forcing;};
 std::vector<Entry> table;
 std::chrono::steady_clock::time_point deadline;
 uint64_t nodes=0,hits=0;
 int negamax(const State &s,int depth,int alpha,int beta,int ply);
 std::vector<Ranked> ordered(const State &s,int ply,Move preferred) const;
 void checkTime() const;
};
}
