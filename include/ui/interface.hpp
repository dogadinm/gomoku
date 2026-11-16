#pragma once
// Terminal interface: draw.cpp, interface.cpp, bench.cpp.
#include "ai/ai.hpp"
namespace gomoku {
// Modes: 1 P1 vs AI, 2 two humans, 3 P2 vs AI, 4 AI vs AI.
bool computer(int mode, int player);
// What the game screen shows besides the position itself.
struct Screen {
    int mode = 1, cx = 9, cy = 9; // cursor on the board
    bool busy = false, explain = false;
    size_t viewing = 0; // actions shown; equals the history size when live
    Move hint;
    SearchResult last;
    const char *message = "Choose a game mode.";
    double elapsed = 0; // of the running AI search
};
void drawMenu(const Rules &selected);
void drawGame(const Session &session, const Session &displayed, const Screen &screen);
int run();
int benchmark(int games);
} // namespace gomoku
