#include "patterns.hpp"
#include <algorithm>
#include <cstdlib>
namespace gomoku {
namespace {
constexpr int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
constexpr unsigned Four = 1, Three = 2, Anchor = 4, FreeThree = 8;
int shape(int n, int open) {
    if (n >= 5)
        return 1000000;
    constexpr int weights[5][3] = {
        {0, 0, 0}, {0, 2, 8}, {0, 30, 150}, {0, 400, 5000}, {0, 25000, 150000}};
    return weights[n][open];
}
struct AxisScore {
    int value = 0;
    uint8_t flags = 0, pairs = 0;
};
struct Tables {
    std::array<std::array<std::array<uint16_t, 8>, 4>, Cells> neighbors{};
    std::array<std::array<AxisScore, 3>, 65536> axes{};
    Tables() {
        for (int cell = 0; cell < Cells; ++cell)
            for (int axis = 0; axis < 4; ++axis) {
                int index = 0;
                for (int off = -4; off <= 4; ++off)
                    if (off) {
                        int x = cell % Size + off * dirs[axis][0],
                            y = cell / Size + off * dirs[axis][1];
                        neighbors[cell][axis][index++] =
                            x < 0 || y < 0 || x >= Size || y >= Size ? Cells : y * Size + x;
                    }
            }
        for (unsigned code = 0; code < 65536; ++code) {
            int v[9]{}, index = 0;
            for (int off = 0; off < 9; ++off)
                if (off != 4) {
                    v[off] = (code >> (index * 2)) & 3u;
                    ++index;
                }
            for (int p : {1, 2}) {
                v[4] = p;
                AxisScore &a = axes[code][p];
                unsigned own = 0, empty = 0;
                for (int i = 0; i < 9; ++i) {
                    own |= unsigned(v[i] == p) << i;
                    empty |= unsigned(v[i] == 0) << i;
                }
                int n = 1, open = 0;
                for (int sign : {-1, 1}) {
                    int k = 1;
                    while (k <= 4 && v[4 + k * sign] == p) {
                        ++n;
                        ++k;
                    }
                    open += k <= 4 && v[4 + k * sign] == 0;
                    if (v[4 + sign] == 3 - p && v[4 + 2 * sign] == 3 - p && v[4 + 3 * sign] == p)
                        ++a.pairs;
                    int b = v[4 + sign], c = v[4 + 2 * sign];
                    if (v[4 + 3 * sign] == 0 &&
                        ((b == 3 - p && (c == 0 || c == 3 - p)) || (c == 3 - p && b == 0)))
                        a.flags |= Anchor;
                }
                a.value = shape(n, open);
                if (n == 4 && open)
                    a.flags |= Four;
                if (n == 3 && open == 2)
                    a.flags |= Three;
                for (int start = 0; start < 4; ++start)
                    if ((empty & (0x21u << start)) == (0x21u << start) &&
                        __builtin_popcount((own >> (start + 1)) & 15u) == 3 &&
                        __builtin_popcount((empty >> (start + 1)) & 15u) == 1)
                        a.flags |= FreeThree;
            }
        }
    }
};
const Tables &tables() {
    static const Tables value;
    return value;
}
} // namespace
MoveAnalyzer::MoveAnalyzer(const State &s) : state(s) {
    std::copy(s.board.begin(), s.board.end(), board.begin());
    board[Cells] = 3;
}
MoveAssessment MoveAnalyzer::inspect(Move m) const {
    MoveAssessment result;
    if (state.winner || state.at(m.x, m.y) != 0)
        return result;
    const Tables &t = tables();
    int cell = m.y * Size + m.x;
    int scores[3]{}, fours[3]{}, threes[3]{}, pairs[3]{}, free[3]{};
    bool anchors[3]{};
    for (int axis = 0; axis < 4; ++axis) {
        unsigned code = 0;
        for (int k = 0; k < 8; ++k)
            code |= unsigned(board[t.neighbors[cell][axis][k]]) << (k * 2);
        for (int p : {1, 2}) {
            const AxisScore &a = t.axes[code][p];
            scores[p] += a.value;
            fours[p] += (a.flags & Four) != 0;
            threes[p] += (a.flags & Three) != 0;
            pairs[p] += a.pairs;
            free[p] += (a.flags & FreeThree) != 0;
            anchors[p] |= (a.flags & Anchor) != 0;
        }
    }
    for (int p : {1, 2}) {
        if (fours[p] >= 2)
            scores[p] += 100000;
        else if (fours[p] && threes[p])
            scores[p] += 18000;
        else if (threes[p] >= 2)
            scores[p] += 6000;
        if (state.rules.captures && pairs[p])
            scores[p] += 2200 * pairs[p] + (state.captured[p] + pairs[p] * 2 >= 10 ? 1000000 : 0);
    }
    result.attack = scores[state.turn];
    result.defense = scores[3 - state.turn];
    result.pairs = state.rules.captures ? pairs[state.turn] : 0;
    result.captureAnchor = state.rules.captures && anchors[state.turn];
    result.legal = !state.rules.doubleThree || result.pairs > 0 || free[state.turn] < 2;
    if (state.rules.opening == Opening::Pro) {
        if (state.moves == 0 && !(m == Move{9, 9}))
            result.legal = false;
        if (state.moves == 2 && std::abs(m.x - 9) <= 2 && std::abs(m.y - 9) <= 2)
            result.legal = false;
    }
    return result;
}
} // namespace gomoku
