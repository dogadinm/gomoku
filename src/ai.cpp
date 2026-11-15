#include "ai.hpp"
#include "patterns.hpp"
#include <algorithm>
#include <cmath>
#include <new>
namespace gomoku {
namespace {
constexpr int Win = 10000000, Infinity = 20000000;
struct Timeout {};
} // namespace
void AI::checkTime() const {
    if (std::chrono::steady_clock::now() >= deadline)
        throw Timeout{};
}
FixedList<AI::Ranked, Cells> AI::ordered(const State &s, int ply, Move preferred) const {
    FixedList<Ranked, Cells> moves, wins;
    MoveList enemyWins;
    MoveAnalyzer analyzer(s);
    State enemy = s;
    enemy.turn = 3 - s.turn;
    for (Move m : s.candidates(false)) {
        if ((moves.size() & 15) == 0)
            checkTime();
        MoveAssessment assessment = analyzer.inspect(m);
        int attack = assessment.attack, defense = assessment.defense;
        if (defense >= 1000000 && !s.pending) {
            State next = enemy;
            if (next.play(m) && next.winner == enemy.turn)
                enemyWins.push_back(m);
        }
        if (!assessment.legal)
            continue;
        bool forcing = attack >= 25000 || defense >= 25000 || s.pending != 0;
        Ranked ranked{m, attack + defense * 9 / 10 + (m == preferred ? 5000000 : 0), forcing,
                      assessment.pairs > 0 || assessment.captureAnchor};
        moves.push_back(ranked);
        if (attack >= 1000000) {
            State next = s;
            next.applyLegal(m);
            if (next.winner == s.turn)
                wins.push_back(ranked);
        }
    }
    // If every nearby square is forbidden, preserve the rules engine's
    // fallback to legal distant squares instead of returning an empty root.
    if (moves.empty()) {
        for (Move m : s.candidates()) {
            checkTime();
            moves.push_back({m, analyzer.inspect(m).attack, false, false});
        }
    }
    // Actual legal wins take precedence over threats and all quiet moves.
    if (!wins.empty())
        return wins;
    if (s.pending || !enemyWins.empty()) {
        FixedList<Ranked, Cells> defenses;
        for (Ranked candidate : moves) {
            // A reply can block the winning square, capture, or introduce an
            // anchor that makes the next five vulnerable to a defensive capture.
            if (!candidate.changesCaptures &&
                (s.pending || enemyWins.size() != 1 || !(candidate.move == enemyWins.front())))
                continue;
            checkTime();
            State next = s;
            next.applyLegal(candidate.move);
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
    std::sort(moves.begin(), moves.end(), [](const Ranked &a, const Ranked &b) {
        if (a.value != b.value)
            return a.value > b.value;
        return a.move.y * Size + a.move.x < b.move.y * Size + b.move.x;
    });
    // Selective search: retain all immediate threats, plus progressively narrower
    // quiet candidates. Reported depth is the completed selective-search depth.
    size_t limit = ply == 0 ? 16 : ply < 3 ? 6 : ply < 6 ? 4 : 2;
    if (moves.size() > limit) {
        FixedList<Ranked, Cells> keep;
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
    Entry cached = table ? table[key % TableSize] : Entry{};
    Move preferred{};
    int originalAlpha = alpha, originalBeta = beta;
    if (cached.key == key && cached.depth >= 0) {
        preferred = cached.best;
        if (cached.depth >= depth) {
            ++hits;
            if (cached.flag == 0) {
                if (ply == 0)
                    rootBest = cached.best;
                return cached.score;
            }
            if (cached.flag == 1)
                alpha = std::max(alpha, cached.score);
            else
                beta = std::min(beta, cached.score);
            if (alpha >= beta) {
                if (ply == 0)
                    rootBest = cached.best;
                return cached.score;
            }
        }
    }
    auto moves = ordered(s, ply, preferred);
    if (moves.empty())
        return 0;
    int best = -Infinity;
    Move bestMove = moves.front().move;
    for (auto candidate : moves) {
        State next = s;
        next.applyLegal(candidate.move);
        int score;
        if (candidate.move == moves.front().move)
            score = -negamax(next, depth - 1, -beta, -alpha, ply + 1);
        else {
            // Principal variation search: first prove that a later move can
            // improve alpha; only then spend a full-window search on it.
            score = -negamax(next, depth - 1, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && score < beta)
                score = -negamax(next, depth - 1, -beta, -alpha, ply + 1);
        }
        if (score > best) {
            best = score;
            bestMove = candidate.move;
        }
        alpha = std::max(alpha, score);
        if (alpha >= beta)
            break;
    }
    if (table)
        table[key % TableSize] = {key, depth, best,
                                  best <= originalAlpha  ? 2
                                  : best >= originalBeta ? 1
                                                         : 0,
                                  bestMove};
    if (ply == 0)
        rootBest = bestMove;
    return best;
}
SearchResult AI::search(const State &s, int milliseconds, int maxDepth,
                        const HistoryProfile &profile) {
    history = profile;
    auto start = std::chrono::steady_clock::now();
    deadline = start + std::chrono::milliseconds(std::max(1, milliseconds));
    nodes = hits = 0;
    if (!table)
        table.reset(new (std::nothrow) Entry[TableSize]);
    if (table)
        std::fill_n(table.get(), TableSize, Entry{});
    SearchResult result;
    result.lowMemory = !table;
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
                    for (int depth = 1; depth <= std::clamp(maxDepth, 1, 64); ++depth) {
                        int window = 2000;
                        int alpha =
                            depth >= 3 ? std::max(-Infinity, result.score - window) : -Infinity;
                        int beta =
                            depth >= 3 ? std::min(Infinity, result.score + window) : Infinity;
                        int score = negamax(s, depth, alpha, beta, 0);
                        if (score <= alpha || score >= beta)
                            score = negamax(s, depth, -Infinity, Infinity, 0);
                        result.move = rootBest;
                        result.score = score;
                        result.depth = depth;
                        if (std::abs(score) >= Win)
                            break;
                    }
            } catch (const Timeout &) {
            }
            State pv = s;
            if (result.depth > 0 && pv.legal(result.move)) {
                result.principal.push_back(result.move);
                pv.play(result.move);
            }
            for (int i = 1; table && i < result.depth && !pv.winner; ++i) {
                const auto &entry = table[pv.hash() % TableSize];
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
    HistoryProfile profile;
    bool lowMemory = false;
    try {
        profile = historyProfile(session);
    } catch (const std::bad_alloc &) {
        lowMemory = true;
    }
    int remaining = std::max(
        1, milliseconds - static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now() - start)
                                               .count()));
    if (!session.openingActive()) {
        auto result = search(session.state, remaining, 12, profile);
        result.lowMemory |= lowMemory;
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
            State next = session.state;
            // Setup placements cannot capture; avoid allocating a copy of
            // the session history merely to evaluate one proposed stone.
            next.board[move.y * Size + move.x] = static_cast<int8_t>(next.turn);
            ++next.moves;
            next.turn = 3 - next.turn;
            int score = std::abs(evaluate(next));
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
    result.lowMemory |= lowMemory;
    result.milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result;
}
} // namespace gomoku
