#include "ai/ai.hpp"
#include <algorithm>
#include <cstdlib>
namespace gomoku {
namespace {
// Value of the shape a player would create on one axis by playing a cell.
constexpr int ShapeValue[ShapeCount] = {0, 1, 4, 10, 40, 60, 400, 500, 8000, 50000};
// Captured pairs grow in value towards the fifth, which wins.
constexpr int CapturedValue[6] = {0, 6000, 14000, 30000, 70000, 70000};
} // namespace
int Evaluation::score(int turn) const {
    int total[3]{};
    for (int p : {1, 2})
        total[p] = alignments[p] + potentialCaptures[p] + captures[p] + figures[p] + dynamic[p] +
                   position[p];
    return std::clamp(total[turn] - total[3 - turn], -5000000, 5000000);
}
int AI::evaluate(const State &s) {
    return inspect(s).score(s.turn);
}
Evaluation AI::inspect(const State &s, const HistoryProfile &profile) {
    return inspect(Board(s), profile);
}
// Every empty cell near the stones is scored from each player's point of view:
// what would a stone there create on each axis (five, open four, four, open
// three...)? A position is as good as the threats it holds in reserve.
Evaluation AI::inspect(const Board &b, const HistoryProfile &profile) {
    Evaluation result;
    int fiveCells[3]{};
    for (int p : {1, 2})
        result.captures[p] = CapturedValue[std::min(b.captured[p] / 2, 5)];
    const int *cells = Board::cells();
    for (int n = 0; n < Cells; ++n) {
        int i = cells[n], v = b.at(i);
        if (v) {
            Move m = Board::move(i);
            result.position[v] += 18 - std::abs(9 - m.x) - std::abs(9 - m.y);
            continue;
        }
        if (!b.near(i))
            continue;
        for (int p : {1, 2}) {
            const CellSummary &c = b.summary(i, p);
            int value = 0, dynamic = 0, pairs = c.pairs;
            for (int axis = 0; axis < 4; ++axis) {
                int shape = ShapeValue[c.shape[axis]];
                value += shape;
                dynamic += std::min(shape, 5000) * std::clamp(profile.axes[p][axis], 0, 8);
            }
            result.alignments[p] += value;
            result.dynamic[p] += dynamic / 32;
            // Figures: one stone creating several threats at once.
            if (c.level == LevelFive)
                ++fiveCells[p];
            else if (c.level == LevelOpenFour)
                result.figures[p] += 20000;
            else if (c.level == LevelFourThree)
                result.figures[p] += 12000;
            else if (c.level == LevelDoubleThree && b.legalFor(i, p))
                result.figures[p] += 5000;
            if (b.rules.captures && pairs) {
                int capture = (2500 + 1500 * b.captured[p] / 2) * pairs + (pairs > 1 ? 2500 : 0);
                if (b.captured[p] + pairs * 2 >= 10)
                    capture += 250000;
                result.potentialCaptures[p] += capture;
                // A capture that also extends a line is a double threat.
                if (c.level >= LevelThree)
                    result.figures[p] += 5000;
                result.dynamic[p] +=
                    std::min(capture, 5000) * std::clamp(profile.captures[p], 0, 8) / 16;
            }
        }
    }
    // Two completion squares cannot both be blocked by one stone.
    for (int p : {1, 2})
        if (fiveCells[p] >= 2)
            result.figures[p] += 100000;
    return result;
}
} // namespace gomoku
