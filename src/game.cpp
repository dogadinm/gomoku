#include "game.hpp"
#include <algorithm>
#include <cstdlib>
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
static constexpr int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
int State::place(Move m) {
    board[m.y * Size + m.x] = static_cast<int8_t>(turn);
    int n = 0;
    if (rules.captures)
        for (auto &d : dirs)
            for (int sign : {-1, 1}) {
                int dx = d[0] * sign, dy = d[1] * sign;
                if (at(m.x + dx, m.y + dy) == 3 - turn &&
                    at(m.x + 2 * dx, m.y + 2 * dy) == 3 - turn &&
                    at(m.x + 3 * dx, m.y + 3 * dy) == turn) {
                    board[(m.y + dy) * Size + m.x + dx] = 0;
                    board[(m.y + 2 * dy) * Size + m.x + 2 * dx] = 0;
                    n += 2;
                }
            }
    captured[turn] += n;
    return n;
}
bool State::doubleThree(Move m) const {
    int axes = 0;
    for (auto &d : dirs) {
        unsigned own = 0, empty = 0;
        for (int off = -4; off <= 4; ++off) {
            int v = at(m.x + off * d[0], m.y + off * d[1]);
            own |= unsigned(v == turn) << (off + 4);
            empty |= unsigned(v == 0) << (off + 4);
        }
        if (__builtin_popcount(own) < 3)
            continue;
        for (int start = 0; start < 4; ++start) {
            // Six cells .XXXX.; the just-played center belongs to the
            // interior for each of these four windows.
            if ((empty & (0x21u << start)) != (0x21u << start))
                continue;
            unsigned stones = (own >> (start + 1)) & 15u, gaps = (empty >> (start + 1)) & 15u;
            if (__builtin_popcount(stones) == 3 && __builtin_popcount(gaps) == 1) {
                if (++axes == 2)
                    return true;
                break;
            }
        }
    }
    return false;
}
bool State::legal(Move m) const {
    if (winner || at(m.x, m.y) != 0)
        return false;
    if (rules.opening == Opening::Pro) {
        if (moves == 0 && !(m == Move{9, 9}))
            return false;
        if (moves == 2 && std::abs(m.x - 9) <= 2 && std::abs(m.y - 9) <= 2)
            return false;
    }
    State s = *this;
    int n = s.place(m);
    return !rules.doubleThree || n > 0 || !s.doubleThree(m);
}
bool State::hasFive(int player) const {
    for (int y = 0; y < Size; ++y)
        for (int x = 0; x < Size; ++x)
            if (at(x, y) == player)
                for (auto &d : dirs) {
                    if (at(x - d[0], y - d[1]) == player)
                        continue;
                    int n = 1;
                    while (at(x + n * d[0], y + n * d[1]) == player)
                        ++n;
                    if (n >= 5)
                        return true;
                }
    return false;
}
bool State::fiveAt(Move m, int player) const {
    for (auto &d : dirs) {
        int count = 1;
        for (int sign : {-1, 1})
            for (int k = 1; at(m.x + k * d[0] * sign, m.y + k * d[1] * sign) == player; ++k)
                ++count;
        if (count >= 5)
            return true;
    }
    return false;
}
bool State::captureDefense(int player) const {
    State enemy = *this;
    enemy.turn = 3 - player;
    enemy.winner = 0;
    // A capture must have an existing anchor. Inspect those anchors rather
    // than copying and simulating the position at every empty intersection.
    for (int y = 0; y < Size; ++y)
        for (int x = 0; x < Size; ++x)
            if (at(x, y) == enemy.turn)
                for (auto &d : dirs)
                    for (int sign : {-1, 1}) {
                        int dx = d[0] * sign, dy = d[1] * sign;
                        if (at(x + dx, y + dy) != player || at(x + 2 * dx, y + 2 * dy) != player ||
                            at(x + 3 * dx, y + 3 * dy) != 0)
                            continue;
                        State s = enemy;
                        s.place({x + 3 * dx, y + 3 * dy});
                        if (s.captured[s.turn] >= 10 || !s.hasFive(player))
                            return true;
                    }
    return false;
}
bool State::play(Move m) {
    if (!legal(m))
        return false;
    applyLegal(m);
    return true;
}
void State::applyLegal(Move m) {
    place(m);
    ++moves;
    if (captured[turn] >= 10)
        winner = turn;
    else if (pending && hasFive(pending))
        winner = pending;
    else if (fiveAt(m, turn)) {
        if (!rules.captures || !captureDefense(turn))
            winner = turn;
        else
            pending = turn;
    } else
        pending = 0;
    turn = 3 - turn;
    if (!winner && std::none_of(board.begin(), board.end(), [](int v) { return v == 0; }))
        winner = 3;
}
MoveList State::candidates(bool legalOnly) const {
    MoveList out;
    bool occupied = false;
    std::array<bool, Cells> near{};
    for (int y = 0; y < Size; ++y)
        for (int x = 0; x < Size; ++x)
            if (at(x, y) > 0) {
                occupied = true;
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dx = -2; dx <= 2; ++dx)
                        if (at(x + dx, y + dy) == 0)
                            near[(y + dy) * Size + x + dx] = true;
            }
    if (!occupied) {
        out.push_back({9, 9});
        return out;
    }
    for (int i = 0; i < Cells; ++i)
        if (near[i] && (!legalOnly || legal({i % Size, i / Size})))
            out.push_back({i % Size, i / Size});
    if (out.empty())
        for (int i = 0; i < Cells; ++i)
            if (at(i % Size, i / Size) == 0 && (!legalOnly || legal({i % Size, i / Size})))
                out.push_back({i % Size, i / Size});
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
