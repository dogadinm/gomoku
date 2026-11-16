#include "ai/search.hpp"
namespace gomoku {
namespace {
// Search widths and quiescence limits.
constexpr int RootWidth = 24, QuietBase = 3, ReduceFrom = 2, ReduceMore = 6;
constexpr int QuiescencePlies = 8, OpenThreePlies = 2, QuiescenceFours = 1, QuiescenceThreats = 3;
// Proven results are stored relative to the node, not the root.
int toTable(int score, int ply) {
    return score >= AI::WinBound ? score + ply : score <= -AI::WinBound ? score - ply : score;
}
int fromTable(int score, int ply) {
    return score >= AI::WinBound ? score - ply : score <= -AI::WinBound ? score + ply : score;
}
} // namespace
int AI::Worker::quiescence(int alpha, int beta, int ply, int qdepth) {
    Board &b = board;
    visit(ply);
    if (b.winner)
        return terminal(b, ply);
    if (ply >= MaxPly || qdepth >= QuiescencePlies)
        return evaluate();
    Candidates moves;
    // Open threes are answered only near the horizon; deeper, only fours.
    int status = generate(moves, qdepth < OpenThreePlies ? Quiescence : Threats, -1, ply);
    if (qdepth >= OpenThreePlies)
        status &= ~Defend;
    int best = -Infinity;
    if (!(status & (Forced | Defend))) {
        // Stand pat: nothing to answer, take the static evaluation.
        best = evaluate();
        if (best >= beta)
            return best;
        alpha = std::max(alpha, best);
    } else if (moves.empty())
        return status & Forced ? -(Win - ply - 2) : evaluate();
    int tried = 0;
    for (size_t n = 0; n < moves.size(); ++n) {
        const Candidate &c = pick(moves, n);
        // Beyond the horizon: up to three double threats, one plain four.
        if (!(status & Forced) &&
            ((c.ours < LevelFourThree && tried >= QuiescenceFours) || tried >= QuiescenceThreats))
            break;
        ++tried;
        Board::Undo undo;
        b.make(c.cell, undo);
        int score = -quiescence(-beta, -alpha, ply + 1, qdepth + 1);
        b.unmake(undo);
        best = std::max(best, score);
        alpha = std::max(alpha, score);
        if (alpha >= beta)
            break;
    }
    return best;
}
// Negamax: a position is worth the best of minus its children's values.
int AI::Worker::negamax(int depth, int alpha, int beta, int ply) {
    Board &b = board;
    visit(ply);
    if (b.winner)
        return terminal(b, ply);
    if (ply >= MaxPly)
        return evaluate();
    // Mate distance pruning: no line can beat a win on the next move.
    alpha = std::max(alpha, -(Win - ply));
    beta = std::min(beta, Win - ply - 1);
    if (alpha >= beta)
        return alpha;
    if (depth <= 0)
        return quiescence(alpha, beta, ply, 0);
    uint64_t key = b.hash();
    Entry entry;
    int ttMove = -1;
    if (ai.probe(key, entry)) {
        ttMove = entry.move;
        // Older entries only order moves (other search, other profile).
        if (ply > 0 && entry.depth >= depth && entry.generation == ai.generation) {
            ++hits;
            int score = fromTable(entry.score, ply);
            if (entry.flag == Exact || (entry.flag == Lower && score >= beta) ||
                (entry.flag == Upper && score <= alpha))
                return score;
        }
    }
    Candidates moves;
    int status = generate(moves, Normal, ttMove, ply, ply == 0 ? MaxPly : depth);
    if (moves.empty())
        return status & Forced ? -(Win - ply - 2) : evaluate();
    int original = alpha, best = -Infinity, bestMove = -1, searched = 0;
    int quiet = ply == 0 ? RootWidth : QuietBase + depth / 2;
    for (size_t n = 0; n < moves.size(); ++n) {
        // Past the quiet limit only forcing moves remain: stop selecting.
        const Candidate &c = searched < quiet ? pick(moves, n) : moves[n];
        bool forcing = (status & Forced) || c.ours >= LevelFour || c.cell == ttMove;
        if (searched >= quiet && !forcing)
            continue;
        // Late move reductions: later quiet moves are searched shallower first.
        int reduction = 0;
        if (depth >= 3 && searched >= ReduceFrom && !(status & Forced) && c.ours < LevelFour &&
            c.theirs < LevelFour && !c.pairs && c.cell != ttMove)
            reduction = searched >= ReduceMore ? 2 : 1;
        Board::Undo undo;
        b.make(c.cell, undo);
        int score;
        if (searched == 0)
            score = -negamax(depth - 1, -beta, -alpha, ply + 1);
        else {
            // PVS: null window first, full re-search only if it beats alpha.
            score = -negamax(depth - 1 - reduction, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && reduction)
                score = -negamax(depth - 1, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && score < beta)
                score = -negamax(depth - 1, -beta, -alpha, ply + 1);
        }
        b.unmake(undo);
        ++searched;
        if (score > best) {
            best = score;
            bestMove = c.cell;
            if (ply == 0)
                rootBest = c.cell;
        }
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            // Remember quiet refutations for sibling nodes.
            if (c.ours < LevelFour && !c.pairs) {
                if (killers[ply][0] != c.cell) {
                    killers[ply][1] = killers[ply][0];
                    killers[ply][0] = c.cell;
                }
                history[b.turn][c.cell] = std::min(history[b.turn][c.cell] + depth * depth, 2000);
            }
            break;
        }
    }
    ai.store(key, toTable(best, ply), bestMove,
             depth, best <= original ? Upper : best >= beta ? Lower : Exact);
    return best;
}
} // namespace gomoku
