#include "game.hpp"
#include <algorithm>
namespace gomoku {
static constexpr int dirs[4][2]={{1,0},{0,1},{1,1},{1,-1}};
int State::place(Move m) {
 board[m.y*Size+m.x]=static_cast<int8_t>(turn);
 int n=0;
 if(rules.captures) for(auto &d:dirs) for(int sign:{-1,1}) {
  int dx=d[0]*sign,dy=d[1]*sign;
  if(at(m.x+dx,m.y+dy)==3-turn&&at(m.x+2*dx,m.y+2*dy)==3-turn&&at(m.x+3*dx,m.y+3*dy)==turn) {
   board[(m.y+dy)*Size+m.x+dx]=0; board[(m.y+2*dy)*Size+m.x+2*dx]=0; n+=2;
  }
 }
 captured[turn]+=n; return n;
}
bool State::doubleThree(Move m) const {
 int axes=0;
 for(auto &d:dirs) {
  bool found=false;
  // An open three can become .XXXX. with one move; both the new stone
  // and the original move must belong to those four interior stones.
  for(int start=-4;start<=0&&!found;++start) {
   if(at(m.x+start*d[0],m.y+start*d[1])!=0 || at(m.x+(start+5)*d[0],m.y+(start+5)*d[1])!=0) continue;
   int stones=0,empty=0; bool includes=false;
   for(int k=1;k<=4;++k) {
    int off=start+k,v=at(m.x+off*d[0],m.y+off*d[1]);
    stones+=v==turn; empty+=v==0; includes|=off==0;
   }
   found=includes&&stones==3&&empty==1;
  }
  axes+=found;
 }
 return axes>=2;
}
bool State::legal(Move m) const {
 if(winner||at(m.x,m.y)!=0) return false;
 State s=*this; int n=s.place(m);
 return !rules.doubleThree||n>0||!s.doubleThree(m);
}
bool State::hasFive(int player) const {
 for(int y=0;y<Size;++y) for(int x=0;x<Size;++x) if(at(x,y)==player)
  for(auto &d:dirs) {
   if(at(x-d[0],y-d[1])==player) continue;
   int n=1; while(at(x+n*d[0],y+n*d[1])==player) ++n;
   if(n>=5) return true;
  }
 return false;
}
bool State::captureDefense(int player) const {
 State enemy=*this; enemy.turn=3-player; enemy.winner=0;
 for(int y=0;y<Size;++y) for(int x=0;x<Size;++x) if(enemy.at(x,y)==0) {
  State s=enemy; int n=s.place({x,y});
  if(n && (s.captured[s.turn]>=10||!s.hasFive(player))) return true;
 }
 return false;
}
bool State::play(Move m) {
 if(!legal(m)) return false;
 place(m); ++moves;
 if(captured[turn]>=10) winner=turn;
 else if(pending && hasFive(pending)) winner=pending;
 else if(hasFive(turn)) {
  if(!rules.captures||!captureDefense(turn)) winner=turn;
  else pending=turn;
 } else pending=0;
 turn=3-turn;
 if(!winner&&std::none_of(board.begin(),board.end(),[](int v){return v==0;})) winner=3;
 return true;
}
std::vector<Move> State::candidates() const {
 std::vector<Move> out; out.reserve(128);
 bool occupied=false; std::array<bool,Cells> near{};
 for(int y=0;y<Size;++y) for(int x=0;x<Size;++x) if(at(x,y)>0) {
  occupied=true;
  for(int dy=-2;dy<=2;++dy) for(int dx=-2;dx<=2;++dx)
   if(at(x+dx,y+dy)==0) near[(y+dy)*Size+x+dx]=true;
 }
 if(!occupied) {out.push_back({9,9});return out;}
 for(int i=0;i<Cells;++i) if(near[i]&&legal({i%Size,i/Size})) out.push_back({i%Size,i/Size});
 if(out.empty()) for(int i=0;i<Cells;++i) if(legal({i%Size,i/Size})) out.push_back({i%Size,i/Size});
 return out;
}
uint64_t State::hash() const {
 uint64_t h=1469598103934665603ULL;
 for(int v:board) {h^=static_cast<uint64_t>(v+1);h*=1099511628211ULL;}
 for(int v:{turn,pending,captured[1],captured[2],int(rules.captures),int(rules.doubleThree)}) {h^=v;h*=1099511628211ULL;}
 return h;
}
}
