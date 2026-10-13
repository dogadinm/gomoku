#include "ai.hpp"
#include "session.hpp"
#include <csignal>
#include <cstring>
#include <future>
#include <iomanip>
#include <iostream>
#include <ncurses.h>
#include <new>
#include <unistd.h>
using namespace gomoku;
namespace {
volatile std::sig_atomic_t stopped = 0;
void stop(int) {
    stopped = 1;
}
struct Terminal {
    Terminal() {
        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        timeout(30);
        curs_set(0);
        mousemask(BUTTON1_CLICKED, nullptr);
        if (has_colors()) {
            start_color();
            use_default_colors();
            init_pair(1, COLOR_CYAN, -1);
            init_pair(2, COLOR_YELLOW, -1);
            init_pair(3, COLOR_GREEN, -1);
        }
    }
    ~Terminal() {
        endwin();
    }
};
const char *side(int p) {
    return p == 1 ? "Black (X)" : "White (O)";
}
bool computer(int mode, int turn) {
    return mode == 4 || (mode == 1 && turn == 2) || (mode == 3 && turn == 1);
}
void draw(const Session &session, const State &s, int cx, int cy, const SearchResult &last,
          const char *message, bool busy, double elapsed, int mode, size_t viewing, Move hint) {
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
    mvprintw(3, 45, "Turn: %s", side(s.turn));
    mvprintw(5, 45, "Captured X: %d / 10", s.captured[1]);
    mvprintw(6, 45, "Captured O: %d / 10", s.captured[2]);
    mvprintw(8, 45, "AI time: %7.1f ms", busy ? elapsed : last.milliseconds);
    mvprintw(9, 45, "Completed depth: %d", last.depth);
    mvprintw(10, 45, "Nodes: %llu", static_cast<unsigned long long>(last.nodes));
    mvprintw(11, 45, "Cache hits: %llu", static_cast<unsigned long long>(last.hits));
    mvprintw(12, 45, "Score: %d", last.score);
    mvprintw(14, 45, "Captures: %s", s.rules.captures ? "on" : "off");
    mvprintw(15, 45, "Double-three ban: %s", s.rules.doubleThree ? "on" : "off");
    mvprintw(17, 45, "Move: %zu / %zu", viewing, session.history.size());
    if (s.winner == 3)
        mvprintw(19, 45, "Draw");
    else if (s.winner)
        mvprintw(19, 45, "Winner: %s", side(s.winner));
    else if (s.pending)
        mvprintw(19, 45, "Five! Capture to defend.");
    else if (busy)
        mvprintw(19, 45, "Thinking...");
    mvprintw(21, 45, "PV:");
    for (size_t i = 0; i < last.principal.size() && i < 5; ++i)
        mvprintw(22, 45 + int(i) * 6, "%c%d", 'A' + last.principal[i].x, last.principal[i].y + 1);
    mvprintw(24, 2, "Arrows/mouse: select  Enter/Space: place  H: hint  U: undo");
    mvprintw(25, 2, "S/L: save/load  [ / ]: replay  P: pause AI  N: new  Q: quit");
    mvaddnstr(26, 2, message, COLS - 4);
    refresh();
}
int benchmark(int games) {
    AI ai;
    double total = 0, max = 0;
    int count = 0, minDepth = 100, deep = 0;
    for (int game = 0; game < games; ++game) {
        State s;
        // Deterministic different openings make timing more useful than an empty board alone.
        s.play({9, 9});
        s.play({8 + game % 3, 10});
        s.play({10, 9});
        s.play({9, 8});
        for (int n = 0; n < 12 && !s.winner; ++n) {
            auto r = ai.search(s);
            if (!s.legal(r.move))
                return 1;
            std::cout << "position=" << count + 1 << " depth=" << r.depth << " ms=" << std::fixed
                      << std::setprecision(1) << r.milliseconds << " nodes=" << r.nodes
                      << " move=" << char('A' + r.move.x) << r.move.y + 1 << '\n';
            total += r.milliseconds;
            max = std::max(max, r.milliseconds);
            minDepth = std::min(minDepth, r.depth);
            deep += r.depth >= 10;
            ++count;
            s.play(r.move);
        }
    }
    std::cout << "Average " << total / count << " ms; maximum " << max << " ms; minimum depth "
              << minDepth << "; depth >=10: " << deep << '/' << count << '\n';
    return 0;
}
int run() {
    Terminal terminal;
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    Session session;
    Rules selected;
    int mode = 1, cx = 9, cy = 9;
    bool menu = true, busy = false, hintJob = false, paused = false;
    size_t viewing = 0;
    Move hint;
    SearchResult last;
    const char *message = "Choose a game mode.";
    std::future<SearchResult> worker;
    std::chrono::steady_clock::time_point started;
    while (!stopped) {
        try {
            if (menu) {
                erase();
                mvprintw(2, 3, "GOMOKU");
                mvprintw(4, 3, "1  Play as Black against AI");
                mvprintw(5, 3, "2  Two humans with AI hints");
                mvprintw(6, 3, "3  Play as White against AI");
                mvprintw(7, 3, "4  Watch AI vs AI");
                mvprintw(9, 3, "C  Captures: %s", selected.captures ? "on" : "off");
                mvprintw(10, 3, "D  Double-three restriction: %s",
                         selected.doubleThree ? "on" : "off");
                mvprintw(12, 3, "Mandatory rules: both options ON. Q quits.");
                mvprintw(14, 3, "Use a terminal of at least 76 x 28 characters.");
                refresh();
                int ch = getch();
                if (ch == 'q' || ch == 'Q')
                    break;
                if (ch == 'c' || ch == 'C')
                    selected.captures = !selected.captures;
                if (ch == 'd' || ch == 'D')
                    selected.doubleThree = !selected.doubleThree;
                if (ch >= '1' && ch <= '4') {
                    mode = ch - '0';
                    session = Session{};
                    session.state.rules = selected;
                    viewing = 0;
                    last = {};
                    hint = {};
                    menu = false;
                    paused = false;
                    message = "Game started.";
                }
                continue;
            }
            if (busy &&
                worker.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                busy = false;
                last = worker.get();
                if (hintJob) {
                    hint = last.move;
                    message = "Suggested move marked with ?.";
                } else {
                    if (session.play(last.move)) {
                        viewing = session.history.size();
                        message = "AI moved.";
                    } else
                        message = "No legal move available.";
                }
            }
            bool live = viewing == session.history.size();
            if (!busy && !paused && live && !session.state.winner &&
                computer(mode, session.state.turn)) {
                State snapshot = session.state;
                started = std::chrono::steady_clock::now();
                worker = std::async(std::launch::async, [snapshot] {
                    AI ai;
                    return ai.search(snapshot);
                });
                busy = true;
                hintJob = false;
                hint = {};
            }
            State displayed = live ? session.state : session.position(viewing);
            double elapsed = std::chrono::duration<double, std::milli>(
                                 std::chrono::steady_clock::now() - started)
                                 .count();
            draw(session, displayed, cx, cy, last, message, busy, elapsed, mode, viewing, hint);
            int ch = getch();
            if (ch == 'q' || ch == 'Q')
                break;
            if (ch == 'p' || ch == 'P') {
                paused = !paused;
                message = paused ? "AI paused after current move." : "AI resumed.";
            }
            if (LINES < 28 || COLS < 76)
                continue;
            if (ch == KEY_LEFT)
                cx = (cx + Size - 1) % Size;
            if (ch == KEY_RIGHT)
                cx = (cx + 1) % Size;
            if (ch == KEY_UP)
                cy = (cy + Size - 1) % Size;
            if (ch == KEY_DOWN)
                cy = (cy + 1) % Size;
            if (busy)
                continue;
            if (ch == 'n' || ch == 'N') {
                menu = true;
                continue;
            }
            if (ch == '[' && viewing > 0) {
                --viewing;
                hint = {};
                continue;
            }
            if (ch == ']' && viewing < session.history.size()) {
                ++viewing;
                continue;
            }
            if (ch == 's' || ch == 'S') {
                message =
                    session.save("gomoku.save") ? "Saved to gomoku.save." : "Cannot save game.";
                continue;
            }
            if (ch == 'l' || ch == 'L') {
                if (session.load("gomoku.save")) {
                    viewing = session.history.size();
                    hint = {};
                    last = {};
                    message = "Loaded gomoku.save.";
                } else
                    message = "Invalid or missing save; current game kept.";
                continue;
            }
            if (ch == 'u' || ch == 'U') {
                session.undo();
                if (mode != 2 && mode != 4 && computer(mode, session.state.turn))
                    session.undo();
                viewing = session.history.size();
                hint = {};
                last = {};
                message = "Move undone.";
                continue;
            }
            if (!live || session.state.winner || computer(mode, session.state.turn))
                continue;
            if (ch == 'h' || ch == 'H') {
                State snapshot = session.state;
                started = std::chrono::steady_clock::now();
                worker = std::async(std::launch::async, [snapshot] {
                    AI ai;
                    return ai.search(snapshot);
                });
                busy = true;
                hintJob = true;
                continue;
            }
            if (ch == KEY_MOUSE) {
                MEVENT event;
                if (getmouse(&event) == OK && event.x >= 4 && event.x <= 40 && event.y >= 4 &&
                    event.y <= 22) {
                    cx = (event.x - 4) / 2;
                    cy = event.y - 4;
                    ch = ' ';
                }
            }
            if (ch == ' ' || ch == '\n' || ch == KEY_ENTER) {
                if (session.play({cx, cy})) {
                    viewing = session.history.size();
                    hint = {};
                    message = "Move played.";
                } else
                    message = "Illegal move: occupied square or forbidden double-three.";
            }
        } catch (const std::bad_alloc &) {
            message = "Not enough memory; try undo or start a new game.";
            mode = 2;
            busy = false;
        } catch (const std::exception &) {
            message = "Operation failed; current game remains available.";
            mode = 2;
            busy = false;
        }
    }
    return 0;
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::strcmp(argv[1], "--bench") == 0)
            return benchmark(2);
        if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
            std::cout << "Gomoku: 19x19 capture Gomoku\nUsage: ./Gomoku [--help | --bench]\nRun in "
                         "a 76x28 or larger terminal. Arrow keys and Space place stones.\n";
            return 0;
        }
        if (argc != 1) {
            std::cerr << "Unknown option; use --help.\n";
            return 1;
        }
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) || !std::getenv("TERM")) {
            std::cerr << "Interactive mode needs a terminal; use --help or --bench.\n";
            return 1;
        }
        return run();
    } catch (const std::exception &e) {
        std::cerr << "Cannot continue: " << e.what() << '\n';
        return 1;
    }
}
