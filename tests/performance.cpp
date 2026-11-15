#include "ai/ai.hpp"
#include <algorithm>
#include <iostream>
#include <random>
using namespace gomoku;
int main() {
    std::mt19937 rng(1701);
    AI ai;
    int count = 0, shallow = 0, terminal = 0, minimum = 100;
    double total = 0, maximum = 0, ongoing = 0;
    for (int game = 0; game < 12; ++game) {
        Session session;
        for (int ply = 1; ply <= 90 && !session.state.winner; ++ply) {
            auto moves = session.state.candidates();
            if (moves.empty())
                break;
            if (!session.play(moves[rng() % moves.size()]))
                return 1;
            if (session.state.winner || (ply != 12 && ply != 24 && ply != 30 && ply != 36 &&
                                         ply != 48 && ply != 60 && ply != 72 && ply != 90))
                continue;
            auto result = ai.act(session);
            if (!session.state.legal(result.move)) {
                std::cerr << "Illegal result\n";
                return 1;
            }
            ++count;
            total += result.milliseconds;
            maximum = std::max(maximum, result.milliseconds);
            bool ends = std::abs(result.score) >= AI::WinBound;
            if (ends)
                ++terminal;
            else {
                ongoing += result.milliseconds;
                minimum = std::min(minimum, result.depth);
                if (result.depth < 10) {
                    ++shallow;
                    session.save("build/shallow-" + std::to_string(game) + "-" +
                                 std::to_string(ply) + ".save");
                }
            }
            std::cout << "game=" << game << " ply=" << ply << " depth=" << result.depth
                      << " ms=" << result.milliseconds << (ends ? " terminal-score" : "") << '\n';
        }
    }
    std::cout << "Positions: " << count << "; terminal-score: " << terminal
              << "; nonterminal minimum depth: " << minimum << "; below 10: " << shallow
              << "; average ms: " << total / count << "; nonterminal average ms: "
              << (count > terminal ? ongoing / (count - terminal) : 0)
              << "; maximum ms: " << maximum << '\n';
    return shallow || !count || total / count > 500 ? 1 : 0;
}
