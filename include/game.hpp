#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace gomoku {
constexpr int Size=19, Cells=Size*Size;
struct Move { int x=-1,y=-1; bool operator==(Move b) const {return x==b.x&&y==b.y;} };
struct Rules { bool captures=true, doubleThree=true; };
struct State {
 std::array<int8_t,Cells> board{};
 std::array<int,3> captured{};
 int turn=1, winner=0, pending=0, moves=0;
 Rules rules{};
 int at(int x,int y) const {return x<0||y<0||x>=Size||y>=Size ? -1 : board[y*Size+x];}
 bool hasFive(int player) const;
 bool legal(Move m) const;
 bool play(Move m);
 std::vector<Move> candidates() const;
 uint64_t hash() const;
private:
 int place(Move m);
 bool doubleThree(Move m) const;
 bool captureDefense(int player) const;
};
}
