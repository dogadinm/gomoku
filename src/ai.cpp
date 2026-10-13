#include "ai.hpp"
#include <algorithm>
#include <cmath>
namespace gomoku {
namespace {
constexpr int Win = 10000000, Infinity = 20000000;
constexpr int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
struct Timeout {};
int shape(int count, int open) {
    if (count >= 5)
        return 1000000;
    if (!open)
        return 0;
    constexpr int weights[5][3] = {
        {0, 0, 0}, {0, 2, 8}, {0, 30, 150}, {0, 400, 5000}, {0, 25000, 150000}};
    return weights[count][open];
}
// Fast move ordering includes attack, defense and capture opportunities.
int potential(const State &s, Move m, int player) {
    int value = 0;
    for (auto &d : dirs) {
        int n = 1, open = 0;
        for (int sign : {-1, 1}) {
            int k = 1;
            while (s.at(m.x + k * d[0] * sign, m.y + k * d[1] * sign) == player) {
                ++n;
                ++k;
            }
            open += s.at(m.x + k * d[0] * sign, m.y + k * d[1] * sign) == 0;
            if (s.rules.captures && s.at(m.x + d[0] * sign, m.y + d[1] * sign) == 3 - player &&
                s.at(m.x + 2 * d[0] * sign, m.y + 2 * d[1] * sign) == 3 - player &&
                s.at(m.x + 3 * d[0] * sign, m.y + 3 * d[1] * sign) == player)
                value += s.captured[player] >= 8 ? 1000000 : 2200;
        }
        value += shape(n, open);
    }
    return value;
}
} // namespace
int AI::evaluate(const State &s) {
    int scores[3] = {0, s.captured[1] * 1600, s.captured[2] * 1600};
    for (int y = 0; y < Size; ++y)
        for (int x = 0; x < Size; ++x) {
            int p = s.at(x, y);
            if (p <= 0)
                continue;
            scores[p] += 18 - std::abs(9 - x) - std::abs(9 - y);
            for (auto &d : dirs) {
                if (s.at(x - d[0], y - d[1]) == p)
                    continue;
                int n = 1;
                while (s.at(x + n * d[0], y + n * d[1]) == p)
                    ++n;
                int open =
                    (s.at(x - d[0], y - d[1]) == 0) + (s.at(x + n * d[0], y + n * d[1]) == 0);
                scores[p] += shape(n, open);
                // Broken lines are less valuable than uninterrupted lines, but not invisible.
                if (n < 4 && s.at(x + n * d[0], y + n * d[1]) == 0) {
                    int k = n + 1;
                    while (s.at(x + k * d[0], y + k * d[1]) == p)
                        ++k;
                    if (k > n + 1 && k - 1 <= 4)
                        scores[p] += shape(k - 1, 1) / 2;
                }
            }
        }
    return std::clamp(scores[s.turn] - scores[3 - s.turn], -Win / 2, Win / 2);
}
void AI::checkTime() const {
    if (std::chrono::steady_clock::now() >= deadline)
        throw Timeout{};
}
std::vector<AI::Ranked> AI::ordered(const State &s, int ply, Move preferred) const {
    std::vector<Ranked> moves;
    for (Move m : s.candidates()) {
        int attack = potential(s, m, s.turn), defense = potential(s, m, 3 - s.turn);
        bool forcing = attack >= 1000000 || defense >= 1000000 || s.pending != 0;
        moves.push_back({m, attack + defense * 9 / 10 + (m == preferred ? 5000000 : 0), forcing});
    }
    std::stable_sort(moves.begin(), moves.end(),
                     [](const Ranked &a, const Ranked &b) { return a.value > b.value; });
    // Selective search: retain all immediate threats, plus progressively narrower
    // quiet candidates. Reported depth is the completed selective-search depth.
    size_t limit = ply == 0 ? 16 : ply < 3 ? 6 : ply < 6 ? 4 : 2;
    if (moves.size() > limit) {
        std::vector<Ranked> keep;
        keep.reserve(limit + 8);
        for (size_t i = 0; i < moves.size(); ++i)
            if (i < limit || moves[i].forcing)
                keep.push_back(moves[i]);
        return keep;
    }
    return moves;
}
int AI::negamax(const State &s, int depth, int alpha, int beta, int ply) {
    ++nodes;
    checkTime();
    if (s.winner)
        return s.winner == 3 ? 0 : s.winner == s.turn ? Win : -Win;
    if (depth == 0)
        return evaluate(s);
    uint64_t key = s.hash();
    Entry &slot = table[key % table.size()];
    Move preferred{};
    int originalAlpha = alpha, originalBeta = beta;
    if (slot.key == key && slot.depth >= 0) {
        preferred = slot.best;
        if (slot.depth >= depth) {
            ++hits;
            if (slot.flag == 0)
                return slot.score;
            if (slot.flag == 1)
                alpha = std::max(alpha, slot.score);
            else
                beta = std::min(beta, slot.score);
            if (alpha >= beta)
                return slot.score;
        }
    }
    auto moves = ordered(s, ply, preferred);
    if (moves.empty())
        return 0;
    int best = -Infinity;
    Move bestMove = moves.front().move;
    for (auto candidate : moves) {
        State next = s;
        next.play(candidate.move);
        int score = -negamax(next, depth - 1, -beta, -alpha, ply + 1);
        if (score > best) {
            best = score;
            bestMove = candidate.move;
        }
        alpha = std::max(alpha, score);
        if (alpha >= beta)
            break;
    }
    slot = {key, depth, best, best <= originalAlpha ? 2 : best >= originalBeta ? 1 : 0, bestMove};
    return best;
}
SearchResult AI::search(const State &s, int milliseconds, int maxDepth) {
    auto start = std::chrono::steady_clock::now();
    deadline = start + std::chrono::milliseconds(std::max(1, milliseconds));
    nodes = hits = 0;
    table.assign(1 << 16, Entry{});
    SearchResult result;
    if (!s.winner) {
        auto available = s.candidates();
        if (!available.empty()) {
            result.move = available.front();
            try {
                auto initial = ordered(s, 0, {});
                result.move = initial.front().move;
                // Recognize every immediate legal win before narrowing the root.
                for (Move m : available) {
                    checkTime();
                    State next = s;
                    next.play(m);
                    if (next.winner == s.turn) {
                        result.move = m;
                        result.score = Win;
                        result.depth = 1;
                        break;
                    }
                }
                if (result.score != Win)
                    for (int depth = 1; depth <= std::max(1, maxDepth); ++depth) {
                        int score = negamax(s, depth, -Infinity, Infinity, 0);
                        Entry entry = table[s.hash() % table.size()];
                        result.move = entry.best;
                        result.score = score;
                        result.depth = depth;
                        if (std::abs(score) >= Win)
                            break;
                    }
            } catch (const Timeout &) {
            }
            State pv = s;
            for (int i = 0; i < result.depth && !pv.winner; ++i) {
                const auto &entry = table[pv.hash() % table.size()];
                if (entry.key != pv.hash() || !pv.legal(entry.best))
                    break;
                result.principal.push_back(entry.best);
                pv.play(entry.best);
            }
        }
    }
    result.nodes = nodes;
    result.hits = hits;
    result.milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result;
}
} // namespace gomoku
