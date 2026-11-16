#include "rules/board.hpp"
#include <algorithm>
namespace gomoku {
const char *openingName(Opening opening) {
    switch (opening) {
    case Opening::Pro:
        return "Pro";
    case Opening::Swap:
        return "Swap";
    case Opening::Swap2:
        return "Swap2";
    default:
        return "Standard";
    }
}
// State is the plain position for the UI and saves; its rules are Board's.
static bool inside(Move m) {
    return m.x >= 0 && m.y >= 0 && m.x < Size && m.y < Size;
}
bool State::legal(Move m) const {
    return inside(m) && Board(*this).legal(Board::index(m));
}
bool State::hasFive(int player) const {
    return Board(*this).hasFive(player);
}
bool State::play(Move m) {
    if (!inside(m))
        return false;
    Board b(*this);
    if (!b.legal(Board::index(m)))
        return false;
    Board::Undo undo;
    b.make(Board::index(m), undo);
    *this = b.state();
    return true;
}
MoveList State::candidates(bool legalOnly) const {
    MoveList out;
    Board b(*this);
    if (!b.stones) {
        out.push_back({9, 9});
        return out;
    }
    // Empty intersections within two cells of a stone; all of them if none.
    for (bool everywhere : {false, true}) {
        for (int n = 0; n < Cells; ++n) {
            int i = Board::cells()[n];
            if (b.at(i) == 0 && (everywhere || b.near(i)) && (!legalOnly || b.legal(i)))
                out.push_back(Board::move(i));
        }
        if (!out.empty())
            break;
    }
    return out;
}
uint64_t State::hash() const {
    uint64_t h = 1469598103934665603ULL;
    for (int v : board) {
        h ^= static_cast<uint64_t>(v + 1);
        h *= 1099511628211ULL;
    }
    for (int v :
         {turn, pending, captured[1], captured[2], int(rules.captures), int(rules.doubleThree),
          int(rules.opening), rules.opening == Opening::Pro ? std::min(moves, 3) : 3}) {
        h ^= v;
        h *= 1099511628211ULL;
    }
    return h;
}
} // namespace gomoku
