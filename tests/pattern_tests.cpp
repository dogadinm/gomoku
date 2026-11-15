#include "rules/board.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace gomoku;
namespace {
constexpr int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
int checks = 0;
void require(bool ok, const char *m) {
    ++checks;
    if (!ok)
        throw std::runtime_error(m);
}
int place(State &s, Move m) {
    s.board[m.y * Size + m.x] = static_cast<int8_t>(s.turn);
    int n = 0;
    if (s.rules.captures)
        for (auto &d : dirs)
            for (int sign : {-1, 1}) {
                int dx = d[0] * sign, dy = d[1] * sign;
                if (s.at(m.x + dx, m.y + dy) == 3 - s.turn &&
                    s.at(m.x + 2 * dx, m.y + 2 * dy) == 3 - s.turn &&
                    s.at(m.x + 3 * dx, m.y + 3 * dy) == s.turn) {
                    s.board[(m.y + dy) * Size + m.x + dx] =
                        s.board[(m.y + 2 * dy) * Size + m.x + 2 * dx] = 0;
                    n += 2;
                }
            }
    s.captured[s.turn] += n;
    return n;
}
bool slowThree(const State &s, Move m) {
    int axes = 0;
    for (auto &d : dirs) {
        bool found = false;
        for (int start = -4; start <= 0 && !found; ++start) {
            if (s.at(m.x + start * d[0], m.y + start * d[1]) != 0 ||
                s.at(m.x + (start + 5) * d[0], m.y + (start + 5) * d[1]) != 0)
                continue;
            int own = 0, empty = 0;
            bool includes = false;
            for (int k = 1; k <= 4; ++k) {
                int offset = start + k, v = s.at(m.x + offset * d[0], m.y + offset * d[1]);
                own += v == s.turn;
                empty += v == 0;
                includes |= offset == 0;
            }
            found = includes && own == 3 && empty == 1;
        }
        axes += found;
    }
    return axes >= 2;
}
bool slowLegal(const State &s, Move m) {
    if (s.winner || s.at(m.x, m.y) != 0)
        return false;
    if (s.rules.opening == Opening::Pro &&
        ((s.moves == 0 && !(m == Move{9, 9})) ||
         (s.moves == 2 && std::abs(m.x - 9) <= 2 && std::abs(m.y - 9) <= 2)))
        return false;
    State after = s;
    int n = place(after, m);
    return !s.rules.doubleThree || n || !slowThree(after, m);
}
bool slowFiveAt(const State &s, Move m, int player) {
    for (auto &d : dirs) {
        int count = 1;
        for (int sign : {-1, 1})
            for (int k = 1; s.at(m.x + k * d[0] * sign, m.y + k * d[1] * sign) == player; ++k)
                ++count;
        if (count >= 5)
            return true;
    }
    return false;
}
bool slowFive(const State &s, int player) {
    for (int y = 0; y < Size; ++y)
        for (int x = 0; x < Size; ++x)
            if (s.at(x, y) == player && slowFiveAt(s, {x, y}, player))
                return true;
    return false;
}
bool slowDefense(const State &s, int player);
// Straightforward reference of a full move: captures, then the win checks in
// the order of the subject (ten captures, unanswered five, new five).
void slowPlay(State &s, Move m) {
    place(s, m);
    ++s.moves;
    int p = s.turn;
    if (s.captured[p] >= 10)
        s.winner = p;
    else if (s.pending && slowFive(s, s.pending))
        s.winner = s.pending;
    else if (slowFiveAt(s, m, p)) {
        if (!s.rules.captures || !slowDefense(s, p))
            s.winner = p;
        else
            s.pending = p;
    } else
        s.pending = 0;
    s.turn = 3 - p;
    if (!s.winner && std::none_of(s.board.begin(), s.board.end(), [](int v) { return v == 0; }))
        s.winner = 3;
}
bool slowDefense(const State &s, int player) {
    for (int y = 0; y < Size; ++y)
        for (int x = 0; x < Size; ++x)
            if (s.at(x, y) == 0) {
                State after = s;
                after.turn = 3 - player;
                int n = place(after, {x, y});
                if (n && (after.captured[after.turn] >= 10 || !slowFive(after, player)))
                    return true;
            }
    return false;
}
void compare(State s, Move m) {
    require(s.legal(m) == slowLegal(s, m), "move legality differs from slow reference");
}
bool sameState(const State &a, const State &b) {
    return a.board == b.board && a.captured == b.captured && a.turn == b.turn &&
           a.winner == b.winner && a.pending == b.pending && a.moves == b.moves;
}
// The incremental board must agree with the slow reference on every rule, and
// unmake must restore everything.
void compareBoard(State s, std::mt19937 &rng, int plies) {
    Board board(s);
    for (int ply = 0; ply < plies && !s.winner; ++ply) {
        require(sameState(board.state(), s), "board mirrors reference");
        MoveList legal;
        for (int i = 0; i < Cells; ++i) {
            Move m{i % Size, i / Size};
            bool expected = slowLegal(s, m);
            require(board.legal(Board::index(m)) == expected, "board legality differs from reference");
            if (expected)
                legal.push_back(m);
        }
        if (legal.empty())
            return;
        uint64_t before = board.hash();
        State saved = board.state();
        Move m = legal[rng() % legal.size()];
        Board::Undo undo;
        board.make(Board::index(m), undo);
        board.unmake(undo);
        require(board.hash() == before && sameState(board.state(), saved),
                "unmake restores board and key");
        // Prefer moves near stones so that captures and fives actually occur.
        auto nearby = s.candidates();
        if (!nearby.empty() && rng() % 4)
            m = nearby[rng() % nearby.size()];
        slowPlay(s, m);
        board.make(Board::index(m), undo);
        require(sameState(board.state(), s), "board move result differs from reference");
        require(Board(s).hash() == board.hash(), "incremental key equals rebuilt key");
    }
}
} // namespace
int main() {
    try {
        // Exhaust every ternary pattern on one axis, crossing a known free three.
        for (int code = 0; code < 6561; ++code) {
            State s;
            s.rules.captures = false;
            int rest = code;
            for (int x = 5; x <= 13; ++x)
                if (x != 9) {
                    s.board[9 * Size + x] = rest % 3;
                    rest /= 3;
                }
            s.board[8 * Size + 9] = s.board[10 * Size + 9] = 1;
            compare(s, {9, 9});
        }
        std::mt19937 rng(42);
        for (int sample = 0; sample < 6000; ++sample) {
            State s;
            s.turn = 1 + rng() % 2;
            s.moves = rng() % 12;
            s.rules.captures = rng() % 2;
            s.rules.doubleThree = rng() % 4 != 0;
            s.rules.opening = sample % 5 == 0 ? Opening::Pro : Opening::Standard;
            Move m{int(rng() % Size), int(rng() % Size)};
            for (int y = 0; y < Size; ++y)
                for (int x = 0; x < Size; ++x)
                    if (rng() % 5 == 0)
                        s.board[y * Size + x] = 1 + rng() % 2;
            s.board[m.y * Size + m.x] = 0;
            compare(s, m);
        }
        for (int sample = 0; sample < 300; ++sample) {
            State s;
            s.turn = 1 + rng() % 2;
            s.captured[1] = 2 * (rng() % 5);
            s.captured[2] = 2 * (rng() % 5);
            for (int i = 0; i < 50; ++i)
                s.board[rng() % Cells] = 1 + rng() % 2;
            auto &d = dirs[sample % 4];
            Move m{9, 9};
            for (int k = 1; k <= 4; ++k)
                s.board[(9 + k * d[1]) * Size + 9 + k * d[0]] = static_cast<int8_t>(s.turn);
            s.board[9 * Size + 9] = 0;
            if (!slowLegal(s, m))
                continue;
            State expected = s;
            slowPlay(expected, m);
            require(s.play(m) && s.hash() == expected.hash() && s.winner == expected.winner,
                    "anchor-based endgame check differs from exhaustive reference");
        }
        for (int game = 0; game < 40; ++game) {
            State s;
            s.rules.captures = game % 5 != 4;
            s.rules.doubleThree = game % 7 != 6;
            s.rules.opening = game % 6 == 0 ? Opening::Pro : Opening::Standard;
            compareBoard(s, rng, 120);
        }
        for (int sample = 0; sample < 400; ++sample) {
            State s;
            s.turn = 1 + rng() % 2;
            s.captured[1] = 2 * (rng() % 5);
            s.captured[2] = 2 * (rng() % 5);
            for (int i = 0; i < 70; ++i)
                s.board[rng() % Cells] = 1 + rng() % 2;
            if (slowFive(s, 1) || slowFive(s, 2))
                continue;
            compareBoard(s, rng, 6);
        }
        std::cout << checks << " reference comparison checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
