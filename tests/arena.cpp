#include "ai.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace gomoku;
int main() {
    int wins = 0, losses = 0, draws = 0, unfinished = 0, early = 0, moves = 0, shallow = 0;
    double total = 0, maximum = 0;
    for (int rivalDepth : {1, 3})
        for (int color : {1, 2})
            for (int opening = 0; opening < 2; ++opening) {
                Session session;
                session.play({9, 9});
                session.play({8 + opening, 10});
                session.play({10, 9});
                session.play({9, 8});
                AI engine, rival;
                int rivalMoves = 0;
                for (int ply = 0; ply < 76 && !session.state.winner; ++ply) {
                    bool ours = session.state.turn == color;
                    SearchResult result = ours ? engine.act(session)
                                               : rival.search(session.state, 100, rivalDepth,
                                                              AI::historyProfile(session));
                    if (!session.state.legal(result.move)) {
                        std::cerr << "Illegal arena move\n";
                        return 1;
                    }
                    if (ours) {
                        ++moves;
                        total += result.milliseconds;
                        maximum = std::max(maximum, result.milliseconds);
                        if (result.depth < 10 && std::abs(result.score) < 10000000)
                            ++shallow;
                    } else
                        ++rivalMoves;
                    session.play(result.move);
                }
                int winner = session.state.winner;
                if (winner == color)
                    ++wins;
                else if (winner == 3)
                    ++draws;
                else if (!winner)
                    ++unfinished;
                else {
                    ++losses;
                    if (rivalMoves < 10)
                        ++early;
                }
                std::string name = "build/arena-" + std::to_string(rivalDepth) + "-" +
                                   std::to_string(color) + "-" + std::to_string(opening) + ".save";
                if (!session.save(name)) {
                    std::cerr << "Cannot save arena replay\n";
                    return 1;
                }
                std::cout << "rival_depth=" << rivalDepth << " AI_color=" << color
                          << " opening=" << opening << " result="
                          << (winner == color ? "AI win"
                              : winner == 3   ? "draw"
                              : !winner       ? "unfinished"
                                              : "AI loss")
                          << " plies=" << session.state.moves << " opponent_moves=" << rivalMoves
                          << std::endl;
            }
    std::cout << "AI wins=" << wins << " losses=" << losses << " draws=" << draws
              << " unfinished=" << unfinished << " early_losses=" << early << " AI_moves=" << moves
              << " below_depth10=" << shallow << " average_ms=" << std::fixed
              << std::setprecision(1) << total / moves << " maximum_ms=" << maximum << '\n';
    std::ofstream report("build/arena-results.txt");
    report << "AI wins: " << wins << "\nAI losses: " << losses << "\nDraws: " << draws
           << "\nUnfinished (80-ply cap): " << unfinished
           << "\nEarly losses (<10 opponent moves): " << early << "\nAI moves: " << moves
           << "\nNonterminal moves below depth 10: " << shallow << "\nAverage ms: " << total / moves
           << "\nMaximum ms: " << maximum << '\n';
    return losses || shallow || total / moves > 500 ? 1 : 0;
}
