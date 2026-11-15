#include "ai/search.hpp"
namespace gomoku {
namespace {
// Far from the horizon (remaining depth >= ThreatDepth) a move that threatens a
// capture is also tried against a five threat.
constexpr int ThreatDepth = 4;
constexpr int OrderValue[ShapeCount] = {0, 1, 4, 8, 30, 40, 400, 500, 6000, 100000};
constexpr int LevelBonus[LevelFive + 1] = {0, 0, 0, 0, 2000, 8000, 20000, 0};
struct View {
    int score = 0;
    uint8_t level = LevelNone, pairs = 0, flags = 0;
};
// Ordering value and tactical facts of a cell for one player.
View view(const Board &b, int i, int player) {
    const CellSummary &c = b.summary(i, player);
    View v;
    v.score = OrderValue[c.shape[0]] + OrderValue[c.shape[1]] + OrderValue[c.shape[2]] +
              OrderValue[c.shape[3]] + LevelBonus[c.level];
    v.level = c.level;
    if (b.rules.captures) {
        v.pairs = c.pairs;
        v.flags = c.flags();
    } else
        v.flags = c.flags() & FreeThreeFlag;
    return v;
}
} // namespace
// Moves are ordered lazily: most nodes cut off after one or two moves, so a
// full sort would be wasted. Brings the best remaining move to position n.
const AI::Candidate &AI::pick(Candidates &moves, size_t n) {
    size_t best = n;
    for (size_t k = n + 1; k < moves.size(); ++k)
        if (moves[k].order > moves[best].order ||
            (moves[k].order == moves[best].order && moves[k].cell < moves[best].cell))
            best = k;
    std::swap(moves[n], moves[best]);
    return moves[n];
}
// Collects the moves worth searching. The tactical situation decides the set:
// a winning move is tried first; facing a five (or a capture win) only answers
// to it are kept; facing an open three only blocks, counter-fours and captures
// are kept. Quiescence and threat search keep only fours.
int AI::Worker::generate(Candidates &out, Mode mode, int ttMove, int ply, int depth) const {
    const Board &b = board;
    int us = b.turn, them = 3 - us, status = 0;
    out.clear();
    if (!b.stones) {
        int center = Board::index({9, 9});
        if (b.legal(center))
            out.push_back({static_cast<int16_t>(center), LevelNone, LevelNone, 0, 0, 0});
        return 0;
    }
    bool threatened = b.pending == them, openThree = false;
    const int *cells = Board::cells();
    for (int n = 0; n < Cells; ++n) {
        int i = cells[n];
        if (b.at(i) || !b.near(i))
            continue;
        if (mode != Normal) {
            // Quiescence and threat search keep only fours, answers to fives
            // and open threes, and captures: skip quiet cells before the full
            // (ordering, legality) work.
            const CellSummary &so = b.summary(i, us), &st = b.summary(i, them);
            if (so.level < LevelFour && st.level < LevelOpenFour &&
                !(b.rules.captures && (so.pairs || st.pairs)))
                continue;
        }
        const View o = view(b, i, us), t = view(b, i, them);
        // A threat counts only if the opponent may legally play it.
        bool theirWin = (t.pairs && b.captured[them] + 2 * t.pairs >= 10) ||
                        (t.level == LevelFive && b.legalFor(i, them));
        threatened |= theirWin;
        openThree |= t.level >= LevelOpenFour && t.level < LevelFive && b.legalFor(i, them);
        if (!b.legal(i))
            continue;
        Candidate c{static_cast<int16_t>(i), o.level, t.level, o.pairs, o.flags, 0};
        bool ourWin = o.level == LevelFive || (o.pairs && b.captured[us] + 2 * o.pairs >= 10);
        if (ourWin) {
            status |= WinNow;
            c.order = 1 << 29;
        } else if (theirWin)
            c.order = 1 << 28;
        else {
            c.order = o.score + t.score * 9 / 10 + history[us][i];
            if (o.pairs)
                c.order += o.pairs * (2500 + 400 * b.captured[us]);
            if (t.pairs)
                c.order += t.pairs * (1500 + 300 * b.captured[them]);
            if (i == killers[ply][0] || i == killers[ply][1])
                c.order += 3000;
        }
        if (i == ttMove)
            c.order = 1 << 30;
        c.flags = static_cast<uint8_t>(o.flags | (theirWin ? TheirWin : 0) | (ourWin ? OurWin : 0));
        out.push_back(c);
    }
    if (threatened)
        status |= Forced;
    else if (openThree)
        status |= Defend;
    size_t kept = 0;
    for (size_t n = 0; n < out.size(); ++n) {
        Candidate c = out[n];
        bool ourWin = c.flags & OurWin;
        bool keep;
        if (b.pending == them)
            keep = c.pairs || ourWin; // only a capture can break the five
        else if (threatened)
            // Far from the horizon a capture threat may also defend: it can
            // make the coming five breakable. Near it, it would widen every
            // forced node.
            keep = ourWin || (c.flags & TheirWin) || c.pairs ||
                   (mode == Normal && depth >= ThreatDepth && (c.flags & ThreatFlag));
        else if (mode == Threats || (mode == Quiescence && !openThree))
            keep = c.ours >= LevelFour;
        else if (mode == Quiescence)
            keep = c.theirs >= LevelOpenFour || c.ours >= LevelFour;
        else if (openThree)
            keep = c.theirs >= LevelFour || c.ours >= LevelFour || c.pairs ||
                   (c.flags & ThreatFlag);
        else
            keep = true;
        if (keep) {
            out[n] = out[kept];
            out[kept++] = c;
        }
    }
    // In the main search an empty answer set means every reply loses (or the
    // filter missed a defense): search all moves rather than none.
    if (kept || mode != Normal)
        out.resize(kept);
    if (out.empty() && mode == Normal) {
        // Every nearby cell is forbidden: fall back to any legal intersection.
        for (int n = 0; n < Cells; ++n) {
            int i = cells[n];
            if (b.legal(i)) {
                out.push_back({static_cast<int16_t>(i), LevelNone, LevelNone, 0, 0, 0});
                break;
            }
        }
    }
    return status;
}
} // namespace gomoku
