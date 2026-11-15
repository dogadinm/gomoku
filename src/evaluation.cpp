#include "ai.hpp"
#include <algorithm>
#include <cstdlib>
namespace gomoku {
namespace {
constexpr int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
int shape(int n, int open) {
    if (n >= 5)
        return 1000000;
    constexpr int weights[5][3] = {
        {0, 0, 0}, {0, 2, 8}, {0, 30, 150}, {0, 400, 5000}, {0, 25000, 150000}};
    return weights[n][open];
}
bool roomForFive(const State &s, int x, int y, int dx, int dy, int p) {
    int cells = 1;
    for (int sign : {-1, 1}) {
        int k = 1;
        while (cells < 5) {
            int v = s.at(x + k * dx * sign, y + k * dy * sign);
            if (v != 0 && v != p)
                break;
            ++cells;
            ++k;
        }
    }
    return cells >= 5;
}
int bits(unsigned int value) {
    return __builtin_popcount(value);
}
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
    Evaluation result;
    // Masks on existing stones identify intersecting threats on different axes.
    std::array<uint8_t, Cells> threes{}, fours{};
    std::array<std::array<uint8_t, Cells>, 3> captureTargets{}, extensions{};
    for (int p : {1, 2})
        result.captures[p] = s.captured[p] * 1600;
    for (int y = 0; y < Size; ++y)
        for (int x = 0; x < Size; ++x) {
            int p = s.at(x, y);
            if (p <= 0)
                continue;
            result.position[p] += 18 - std::abs(9 - x) - std::abs(9 - y);
            for (int axis = 0; axis < 4; ++axis) {
                int dx = dirs[axis][0], dy = dirs[axis][1];
                if (s.rules.captures)
                    for (int sign : {-1, 1}) {
                        if (s.at(x + sign * dx, y + sign * dy) == 3 - p &&
                            s.at(x + 2 * sign * dx, y + 2 * sign * dy) == 3 - p &&
                            s.at(x + 3 * sign * dx, y + 3 * sign * dy) == 0)
                            ++captureTargets[p][(y + 3 * sign * dy) * Size + x + 3 * sign * dx];
                    }
                if (s.at(x - dx, y - dy) == p)
                    continue;
                int n = 1;
                while (s.at(x + n * dx, y + n * dy) == p)
                    ++n;
                int open = (s.at(x - dx, y - dy) == 0) + (s.at(x + n * dx, y + n * dy) == 0);
                if (n < 5 && (!open || !roomForFive(s, x, y, dx, dy, p)))
                    continue;
                int value = shape(n, open);
                int length = n;
                bool isFour = n == 4 && open > 0, isThree = n == 3 && open == 2;
                // One-gap lines need space too; a four with an internal gap has
                // one completion, regardless of whether its outside ends are open.
                if (n < 4 && s.at(x + n * dx, y + n * dy) == 0) {
                    int k = n + 1;
                    while (s.at(x + k * dx, y + k * dy) == p)
                        ++k;
                    int stones = k - 1;
                    if (k > n + 1 && stones <= 4) {
                        int brokenOpen =
                            (s.at(x - dx, y - dy) == 0) + (s.at(x + k * dx, y + k * dy) == 0);
                        int broken = stones == 4 ? 25000 : shape(stones, brokenOpen) / 2;
                        value += broken;
                        length = k;
                        isFour |= stones == 4;
                        isThree |= stones == 3 && brokenOpen == 2;
                    }
                }
                result.alignments[p] += value;
                result.dynamic[p] +=
                    std::min(value, 5000) * std::clamp(profile.axes[p][axis], 0, 8) / 32;
                if (isFour || isThree) {
                    for (int k = 0; k < length; ++k) {
                        int cell = (y + k * dy) * Size + x + k * dx;
                        if (s.board[cell] == p) {
                            if (isFour)
                                fours[cell] |= 1 << axis;
                            if (isThree)
                                threes[cell] |= 1 << axis;
                        } else if (s.board[cell] == 0)
                            extensions[p][cell] |= 1 << axis;
                    }
                    for (int k : {-1, length})
                        if (s.at(x + k * dx, y + k * dy) == 0)
                            extensions[p][(y + k * dy) * Size + x + k * dx] |= 1 << axis;
                }
            }
        }
    for (int cell = 0; cell < Cells; ++cell) {
        int p = s.board[cell];
        if (p > 0) {
            int four = bits(fours[cell]), three = bits(threes[cell] & ~fours[cell]);
            if (four >= 2)
                result.figures[p] += 100000;
            else if (four && three)
                result.figures[p] += 18000;
            else if (three >= 2)
                result.figures[p] += 6000;
        }
        for (int player : {1, 2})
            if (captureTargets[player][cell]) {
                int pairs = captureTargets[player][cell];
                int value = 1800 * pairs + (pairs > 1 ? 2500 : 0);
                if (s.captured[player] + pairs * 2 >= 10)
                    value += 250000;
                result.potentialCaptures[player] += value;
                if (extensions[player][cell])
                    result.figures[player] += 5000;
                result.dynamic[player] +=
                    std::min(value, 5000) * std::clamp(profile.captures[player], 0, 8) / 16;
            }
    }
    return result;
}
HistoryProfile AI::historyProfile(const Session &session) {
    HistoryProfile profile;
    // Real completed actions only; hypothetical search branches never train it.
    size_t begin = session.history.size() > 12 ? session.history.size() - 12 : 0;
    Session past = session.replay(begin);
    std::array<Move, 3> previous{};
    for (size_t i = begin; i < session.history.size(); ++i) {
        const Action &action = session.history[i];
        int p = past.state.turn;
        bool setup = past.openingActive();
        int captured = past.state.captured[p];
        if (!past.apply(action))
            break;
        if (action.choice != Choice::None || setup)
            continue;
        // Recent actions carry greater weight (1..4).
        int weight = 1 + static_cast<int>((i - begin) / 3);
        profile.captures[p] =
            std::min(8, profile.captures[p] + (past.state.captured[p] - captured) / 2 * weight);
        Move old = previous[p];
        if (old.x >= 0) {
            int dx = action.move.x - old.x, dy = action.move.y - old.y;
            if (std::max(std::abs(dx), std::abs(dy)) <= 4) {
                int axis = dy == 0 ? 0 : dx == 0 ? 1 : dx == dy ? 2 : dx == -dy ? 3 : -1;
                if (axis >= 0)
                    profile.axes[p][axis] = std::min(8, profile.axes[p][axis] + weight);
            }
        }
        previous[p] = action.move;
    }
    return profile;
}
} // namespace gomoku
