#include "ui/interface.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
namespace gomoku {
// Plays 12 AI moves from each deterministic opening and prints depth and time.
int benchmark(int games) {
    AI ai;
    double total = 0, max = 0;
    int count = 0, open = 0, minDepth = 100, deep = 0;
    for (int game = 0; game < games; ++game) {
        Session session;
        State &s = session.state;
        session.play({9, 9});
        session.play({8 + game % 3, 10});
        session.play({10, 9});
        session.play({9, 8});
        for (int n = 0; n < 12 && !s.winner; ++n) {
            auto r = ai.act(session);
            if (!s.legal(r.move))
                return 1;
            // A proven win or loss ends the search before the minimum depth.
            bool proven = std::abs(r.score) >= AI::WinBound;
            std::cout << "position=" << count + 1 << " depth=" << r.depth << " ms=" << std::fixed
                      << std::setprecision(1) << r.milliseconds << " nodes=" << r.nodes
                      << " move=" << char('A' + r.move.x) << r.move.y + 1
                      << (proven ? " proven" : "") << '\n';
            total += r.milliseconds;
            max = std::max(max, r.milliseconds);
            if (!proven) {
                minDepth = std::min(minDepth, r.depth);
                deep += r.depth >= AI::MinimumDepth;
                ++open;
            }
            ++count;
            session.play(r.move);
        }
    }
    std::cout << "Average " << total / count << " ms; maximum " << max
              << " ms; unproven positions: minimum depth " << minDepth << ", depth >=10: " << deep
              << '/' << open << '\n';
    return 0;
}
} // namespace gomoku
