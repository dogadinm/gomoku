#include "rules/patterns.hpp"
#include <algorithm>
namespace gomoku {
namespace {
void decode(unsigned code, int player, int v[9]) {
    for (int k = 0; k < 8; ++k)
        v[offsetOf(k) + 4] = (code >> (2 * k)) & 3u;
    v[4] = player;
}
int runThroughCenter(const int v[9], int player) {
    int n = 1;
    for (int i = 3; i >= 0 && v[i] == player; --i)
        ++n;
    for (int i = 5; i < 9 && v[i] == player; ++i)
        ++n;
    return n;
}
} // namespace
// Recursive: a four completes five in one move (Flex: in two ways), a three
// makes a four, and so on.
int Tables::classify(unsigned code, int player) {
    int8_t &m = memo[player - 1][code];
    if (m >= 0)
        return m;
    int v[9];
    decode(code, player, v);
    if (runThroughCenter(v, player) >= 5)
        return m = Five;
    int completions = 0;
    for (int i = 0; i < 9; ++i)
        if (v[i] == 0) {
            v[i] = player;
            completions += runThroughCenter(v, player) >= 5;
            v[i] = 0;
        }
    if (completions)
        return m = completions >= 2 ? Flex4 : Block4;
    int best = ShapeNone;
    for (int k = 0; k < 8; ++k)
        if (((code >> (2 * k)) & 3u) == 0) {
            int child = classify(code | unsigned(player) << (2 * k), player);
            // Each extra stone needed lowers the shape by one level.
            int lowered = child >= Block2 ? child - 2 : ShapeNone;
            best = std::max(best, lowered);
        }
    return m = best;
}
Tables::Tables() {
    for (auto &row : memo)
        row.fill(-1);
    uint64_t seed = 0x9E3779B97F4A7C15ULL;
    auto random = [&seed] {
        uint64_t z = (seed += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    };
    for (int p : {1, 2})
        for (auto &z : zobrist[p])
            z = random();
    for (auto &z : pending)
        z = random();
    for (int p : {1, 2})
        for (auto &z : captured[p])
            z = random();
    turn = random();
    for (auto &z : rules)
        z = random();
    for (int i = 0; i < Cells; ++i)
        cells[i] = Board::index({i % Size, i / Size});
    // Level of a cell from the shapes on its four axes.
    for (size_t n = 0; n < levels.size(); ++n) {
        int fours = 0, flex4 = 0, flex3 = 0, five = 0, low = 0;
        for (size_t rest = n, k = 0; k < 4; ++k, rest /= ShapeCount) {
            int shape = static_cast<int>(rest % ShapeCount);
            five |= shape == Five;
            fours += shape == Block4 || shape == Flex4;
            flex4 += shape == Flex4;
            flex3 += shape == Flex3;
            low |= shape != ShapeNone;
        }
        levels[n] = five               ? LevelFive
                    : flex4 || fours > 1 ? LevelOpenFour
                    : fours && flex3     ? LevelFourThree
                    : fours              ? LevelFour
                    : flex3 > 1          ? LevelDoubleThree
                    : flex3              ? LevelThree
                    : low                ? LevelLow
                                         : LevelNone;
    }
    for (unsigned code = 0; code < 65536; ++code)
        for (int p : {1, 2}) {
            int v[9];
            decode(code, p, v);
            LineInfo &a = info[p - 1][code];
            a.shape = static_cast<uint8_t>(classify(code, p));
            unsigned own = 0, empty = 0;
            for (int i = 0; i < 9; ++i) {
                own |= unsigned(v[i] == p) << i;
                empty |= unsigned(v[i] == 0) << i;
            }
            for (int sign : {-1, 1}) {
                int b = v[4 + sign], c = v[4 + 2 * sign], d = v[4 + 3 * sign];
                if (b == 3 - p && c == 3 - p && d == p)
                    ++a.pairs;
                if (b == 3 - p && c == 3 - p && d == 0)
                    a.flags |= ThreatFlag;
            }
            // Free three (rule): window .XXXX. with three stones and one gap inside.
            for (int start = 0; start < 4; ++start)
                if ((empty & (0x21u << start)) == (0x21u << start) &&
                    __builtin_popcount((own >> (start + 1)) & 15u) == 3 &&
                    __builtin_popcount((empty >> (start + 1)) & 15u) == 1)
                    a.flags |= FreeThreeFlag;
        }
}
} // namespace gomoku
