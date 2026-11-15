#include "ui/interface.hpp"
#include <ncurses.h>
namespace gomoku {
namespace {
const char *side(int p) {
    return p == 1 ? "Black (X)" : "White (O)";
}
} // namespace
void drawMenu(const Rules &selected) {
    erase();
    mvprintw(2, 3, "GOMOKU");
    mvprintw(4, 3, "1  Player 1 (Black before swap) vs AI");
    mvprintw(5, 3, "2  Two humans with AI hints");
    mvprintw(6, 3, "3  Player 2 (White before swap) vs AI");
    mvprintw(7, 3, "4  Watch AI vs AI");
    mvprintw(9, 3, "C  Captures: %s", selected.captures ? "on" : "off");
    mvprintw(10, 3, "D  Double-three restriction: %s", selected.doubleThree ? "on" : "off");
    mvprintw(11, 3, "O  Opening: %s", openingName(selected.opening));
    mvprintw(12, 3, "Mandatory: Standard, both rules ON. Q quits.");
    mvprintw(16, 3, "Swap: P1 sets 3 stones; P2 chooses a color.");
    mvprintw(17, 3, "Swap2: P2 may add 2 stones, then P1 chooses.");
    mvprintw(14, 3, "Use a terminal of at least 76 x 28 characters.");
    refresh();
}
void drawGame(const Session &session, const Session &displayed, const Screen &screen) {
    const State &s = displayed.state;
    const SearchResult &last = screen.last;
    int cx = screen.cx, cy = screen.cy, mode = screen.mode;
    Move hint = screen.hint;
    erase();
    if (LINES < 28 || COLS < 76) {
        mvprintw(0, 0, "Please resize terminal to at least 76 columns x 28 rows.");
        mvprintw(2, 0, "Current: %d x %d. Q exits.", COLS, LINES);
        refresh();
        return;
    }
    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(0, 2, "GOMOKU / 19 x 19");
    attroff(A_BOLD | COLOR_PAIR(1));
    mvprintw(1, 2, "%s", mode == 2 ? "Human vs Human" : mode == 4 ? "AI vs AI" : "Human vs AI");
    for (int x = 0; x < Size; ++x)
        mvprintw(3, 4 + x * 2, "%c", 'A' + x);
    for (int y = 0; y < Size; ++y) {
        mvprintw(4 + y, 0, "%2d", y + 1);
        for (int x = 0; x < Size; ++x) {
            int v = s.at(x, y);
            bool cursor = x == cx && y == cy;
            bool suggested = hint == Move{x, y};
            if (cursor)
                attron(A_REVERSE);
            if (v)
                attron(COLOR_PAIR(v) | A_BOLD);
            else if (suggested)
                attron(COLOR_PAIR(3) | A_BOLD);
            mvaddch(4 + y, 4 + x * 2, v == 1 ? 'X' : v == 2 ? 'O' : suggested ? '?' : '+');
            if (v)
                attroff(COLOR_PAIR(v) | A_BOLD);
            else if (suggested)
                attroff(COLOR_PAIR(3) | A_BOLD);
            if (cursor)
                attroff(A_REVERSE);
        }
    }
    mvprintw(2, 45, "Opening: %s", openingName(s.rules.opening));
    mvprintw(3, 45, "Player %d: %s", displayed.actor(), side(s.turn));
    if (!displayed.openingActive())
        mvprintw(4, 45, "Black: P%d / White: P%d",
                 displayed.blackPlayer ? displayed.blackPlayer : 1,
                 displayed.blackPlayer ? 3 - displayed.blackPlayer : 2);
    mvprintw(5, 45, "Captured X: %d / 10", s.captured[1]);
    mvprintw(6, 45, "Captured O: %d / 10", s.captured[2]);
    mvprintw(8, 45, "AI time: %7.1f ms", screen.busy ? screen.elapsed : last.milliseconds);
    mvprintw(9, 45, "Depth: %d (selective %d)%s", last.depth, last.selective,
             last.threat ? " VCF" : "");
    mvprintw(10, 45, "Nodes: %llu (%d threads)", static_cast<unsigned long long>(last.nodes),
             last.threads);
    mvprintw(11, 45, "Cache hits: %llu", static_cast<unsigned long long>(last.hits));
    mvprintw(12, 45, "Score: %d", last.score);
    mvprintw(14, 45, "Captures: %s", s.rules.captures ? "on" : "off");
    mvprintw(15, 45, "Double-three ban: %s", s.rules.doubleThree ? "on" : "off");
    if (screen.explain) {
        Evaluation values = AI::inspect(s, AI::historyProfile(displayed));
        const std::array<int, 3> *parts[] = {&values.alignments, &values.potentialCaptures,
                                             &values.captures,   &values.figures,
                                             &values.dynamic,    &values.position};
        const char *labels[] = {"Lines", "Potential", "Captured", "Figures", "History", "Center"};
        for (int i = 0; i < 6; ++i) {
            move(10 + i, 45);
            clrtoeol();
            mvprintw(10 + i, 45, "%-9s X:%d O:%d", labels[i], (*parts[i])[1], (*parts[i])[2]);
        }
    }
    if (last.lowMemory)
        mvprintw(16, 45, "Memory fallback active");
    mvprintw(17, 45, "Action: %zu / %zu", screen.viewing, session.history.size());
    if (s.winner == 3)
        mvprintw(19, 45, "Draw");
    else if (s.winner)
        mvprintw(19, 45, "Winner: %s", side(s.winner));
    else if (s.pending)
        mvprintw(19, 45, "Five! Capture to defend.");
    else if (screen.busy)
        mvprintw(19, 45, "Thinking...");
    if (displayed.openingActive()) {
        if (displayed.needsChoice()) {
            mvprintw(18, 45, "P%d chooses color", displayed.actor());
            mvprintw(20, 45, "B: Black / W: White");
            if (s.rules.opening == Opening::Swap2 && !displayed.extended)
                mvprintw(21, 45, "E: place two more stones");
        } else {
            mvprintw(18, 45, "P%d places setup stone %d/%d", displayed.actor(), s.moves + 1,
                     displayed.extended ? 5 : 3);
        }
    } else
        mvprintw(21, 45, "PV:");
    for (size_t i = 0; i < last.principal.size() && i < 5; ++i)
        mvprintw(22, 45 + int(i) * 6, "%c%d", 'A' + last.principal[i].x, last.principal[i].y + 1);
    mvprintw(24, 2, "Arrows/mouse: select  Enter/Space: place  H: hint  U: undo");
    mvprintw(25, 2, "S/L: save/load  [/]: replay  V: evaluation  P: pause  N: new  Q: quit");
    mvaddnstr(26, 2, screen.message, COLS - 4);
    refresh();
}
} // namespace gomoku
