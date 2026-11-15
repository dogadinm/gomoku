#include "patterns.hpp"
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
bool slowDefense(const State &s, int player) {
    for (int y = 0; y < Size; ++y)
        for (int x = 0; x < Size; ++x)
            if (s.at(x, y) == 0) {
                State after = s;
                after.turn = 3 - player;
                int n = place(after, {x, y});
                if (n && (after.captured[after.turn] >= 10 || !after.hasFive(player)))
                    return true;
            }
    return false;
}
void compare(State s, Move m) {
    MoveAnalyzer analyzer(s);
    bool expected = slowLegal(s, m);
    require(analyzer.inspect(m).legal == expected,
            "lookup move legality differs from slow reference");
    require(s.legal(m) == expected, "bit-mask double-three differs from slow reference");
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
            place(expected, m);
            ++expected.moves;
            if (expected.captured[expected.turn] >= 10)
                expected.winner = expected.turn;
            else if (expected.hasFive(expected.turn)) {
                if (!slowDefense(expected, expected.turn))
                    expected.winner = expected.turn;
                else
                    expected.pending = expected.turn;
            }
            expected.turn = 3 - expected.turn;
            require(s.play(m) && s.hash() == expected.hash() && s.winner == expected.winner,
                    "anchor-based endgame check differs from exhaustive reference");
        }
        std::cout << checks << " reference comparison checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
