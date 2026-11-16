#include "ai/search.hpp"
namespace gomoku {
// VCF: true if fours alone win against every defense within `depth` moves.
bool AI::Worker::vcf(int attacker, int depth, long &budget, int ply) {
    Board &b = board;
    if (--budget < 0 || ply >= MaxPly - 2)
        return false;
    visit(ply);
    Candidates moves;
    int status = generate(moves, Threats, -1, ply);
    if (status & WinNow)
        for (const Candidate &c : moves) {
            Board::Undo undo;
            b.make(c.cell, undo);
            bool won = b.winner == attacker;
            b.unmake(undo);
            if (won) {
                if (ply == 0)
                    rootBest = c.cell;
                return true;
            }
        }
    if ((status & Forced) || depth <= 0)
        return false;
    for (size_t n = 0; n < moves.size(); ++n) {
        const Candidate &c = pick(moves, n);
        Board::Undo undo;
        b.make(c.cell, undo);
        bool proven = b.winner == attacker;
        if (!b.winner) {
            Candidates replies;
            proven = generate(replies, Normal, -1, ply + 1, MaxPly) & Forced;
            for (size_t k = 0; proven && k < replies.size(); ++k) {
                Board::Undo answer;
                b.make(replies[k].cell, answer);
                proven = b.winner == attacker ||
                         (!b.winner && vcf(attacker, depth - 1, budget, ply + 2));
                b.unmake(answer);
            }
        }
        b.unmake(undo);
        if (proven) {
            if (ply == 0)
                rootBest = c.cell;
            return true;
        }
    }
    return false;
}
} // namespace gomoku
