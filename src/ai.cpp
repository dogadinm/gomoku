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
int potential(const State &s, Move m, int player, int *capturePairs = nullptr,
              bool *captureAnchor = nullptr) {
    if (captureAnchor)
        *captureAnchor = false;
    int value = 0, fours = 0, threes = 0, pairs = 0;
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
                ++pairs;
            if (s.rules.captures && captureAnchor &&
                s.at(m.x + 3 * d[0] * sign, m.y + 3 * d[1] * sign) == 0) {
                int a = s.at(m.x + d[0] * sign, m.y + d[1] * sign),
                    b = s.at(m.x + 2 * d[0] * sign, m.y + 2 * d[1] * sign);
                if ((a == 3 - player && (b == 0 || b == 3 - player)) || (b == 3 - player && a == 0))
                    *captureAnchor = true;
            }
        }
        value += shape(n, open);
        fours += n == 4 && open > 0;
        threes += n == 3 && open == 2;
    }
    if (fours >= 2)
        value += 100000;
    else if (fours && threes)
        value += 18000;
    else if (threes >= 2)
        value += 6000;
    if (pairs)
        value += 2200 * pairs + (s.captured[player] + pairs * 2 >= 10 ? 1000000 : 0);
    if (capturePairs)
        *capturePairs = pairs;
    return value;
}
} // namespace
void AI::checkTime() const {
    if (std::chrono::steady_clock::now() >= deadline)
        throw Timeout{};
}
std::vector<AI::Ranked> AI::ordered(const State &s, int ply, Move preferred) const {
    std::vector<Ranked> moves, wins;
    std::vector<Move> enemyWins;
    State enemy = s;
    enemy.turn = 3 - s.turn;
    for (Move m : s.candidates(false)) {
        if ((moves.size() & 15) == 0)
            checkTime();
        int pairs = 0;
        bool anchor = false;
        int attack = potential(s, m, s.turn, &pairs, &anchor),
            defense = potential(s, m, 3 - s.turn);
        if (defense >= 1000000 && !s.pending) {
            State next = enemy;
            if (next.play(m) && next.winner == enemy.turn)
                enemyWins.push_back(m);
        }
        if (!s.legal(m))
            continue;
        bool forcing = attack >= 1000000 || defense >= 1000000 || s.pending != 0;
        Ranked ranked{m, attack + defense * 9 / 10 + (m == preferred ? 5000000 : 0), forcing,
                      pairs > 0 || anchor};
        moves.push_back(ranked);
        if (attack >= 1000000) {
            State next = s;
            if (next.play(m) && next.winner == s.turn)
                wins.push_back(ranked);
        }
    }
    // If every nearby square is forbidden, preserve the rules engine's
    // fallback to legal distant squares instead of returning an empty root.
    if (moves.empty()) {
        for (Move m : s.candidates()) {
            checkTime();
            moves.push_back({m, potential(s, m, s.turn), false, false});
        }
    }
    // Actual legal wins take precedence over threats and all quiet moves.
    if (!wins.empty())
        return wins;
    if (s.pending || !enemyWins.empty()) {
        std::vector<Ranked> defenses;
        for (Ranked candidate : moves) {
            // A reply can block the winning square, capture, or introduce an
            // anchor that makes the next five vulnerable to a defensive capture.
            if (!candidate.changesCaptures &&
                (s.pending || enemyWins.size() != 1 || !(candidate.move == enemyWins.front())))
                continue;
            checkTime();
            State next = s;
            next.play(candidate.move);
            bool loses = next.winner == 3 - s.turn;
            for (Move threat : enemyWins) {
                State reply = next;
                if (reply.play(threat) && reply.winner == 3 - s.turn) {
                    loses = true;
                    break;
                }
            }
            if (!loses) {
                candidate.forcing = true;
                defenses.push_back(candidate);
            }
        }
        if (!defenses.empty())
            moves = std::move(defenses);
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
        return inspect(s, history).score(s.turn);
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
SearchResult AI::search(const State &s, int milliseconds, int maxDepth,
                        const HistoryProfile &profile) {
    history = profile;
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
SearchResult AI::act(const Session &session, int milliseconds) {
    auto start = std::chrono::steady_clock::now();
    HistoryProfile profile = historyProfile(session);
    int remaining = std::max(
        1, milliseconds - static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now() - start)
                                               .count()));
    if (!session.openingActive()) {
        auto result = search(session.state, remaining, 12, profile);
        result.milliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        return result;
    }
    SearchResult result;
    if (session.needsChoice()) {
        // Both choices are available: take the color favored by a normal search.
        result = search(session.state, remaining, 12, profile);
        int favored = result.score >= 0 ? session.state.turn : 3 - session.state.turn;
        result.choice = favored == 1 ? Choice::Black : Choice::White;
        result.move = {};
        result.principal.clear();
    } else {
        // The opponent may choose either color: propose a balanced setup.
        int best = Infinity;
        for (Move move : session.state.candidates()) {
            Session next = session;
            if (!next.play(move))
                continue;
            int score = std::abs(evaluate(next.state));
            int distance = std::abs(move.x - 9) + std::abs(move.y - 9);
            int rank = score * 100 + distance;
            if (rank < best) {
                best = rank;
                result.move = move;
                result.score = score;
            }
            ++result.nodes;
            if (std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(milliseconds))
                break;
        }
    }
    result.milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result;
}
} // namespace gomoku
