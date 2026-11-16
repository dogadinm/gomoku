#include "ui/interface.hpp"
#include <csignal>
#include <future>
#include <ncurses.h>
#include <new>
#include <optional>
namespace gomoku {
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
// The interactive game; the AI searches in a background thread.
class Game {
  public:
    bool step(); // false when the user quits
    void recover(const char *message);

  private:
    Session session;
    Rules selected;
    Screen screen;
    bool menu = true, paused = false, hintJob = false;
    // One engine for the run (its table persists); declared before the worker.
    AI engine;
    std::future<SearchResult> worker;
    std::chrono::steady_clock::time_point started;
    bool menuStep();
    void collect();
    void think(bool hint);
    bool handleKey(int ch, bool live);
};
bool Game::menuStep() {
    drawMenu(selected);
    int ch = getch();
    if (ch == 'q' || ch == 'Q')
        return false;
    if (ch == 'c' || ch == 'C')
        selected.captures = !selected.captures;
    if (ch == 'd' || ch == 'D')
        selected.doubleThree = !selected.doubleThree;
    if (ch == 'o' || ch == 'O')
        selected.opening = static_cast<Opening>((static_cast<int>(selected.opening) + 1) % 4);
    if (ch >= '1' && ch <= '4') {
        screen.mode = ch - '0';
        session = Session{};
        session.state.rules = selected;
        screen.cx = screen.cy = 9;
        screen.viewing = 0;
        screen.last = {};
        screen.hint = {};
        menu = false;
        paused = false;
        screen.message = "Game started.";
    }
    return true;
}
// Takes the result of a finished search: a hint is only shown, a move is played.
void Game::collect() {
    if (!screen.busy ||
        worker.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        return;
    screen.busy = false;
    screen.last = worker.get();
    if (hintJob) {
        screen.hint = screen.last.move;
        screen.message = screen.last.choice == Choice::Black   ? "Suggested choice: Black (B)."
                         : screen.last.choice == Choice::White ? "Suggested choice: White (W)."
                                                               : "Suggested move marked with ?.";
    } else if (session.apply({screen.last.move, screen.last.choice})) {
        screen.viewing = session.history.size();
        screen.message = "AI moved.";
    } else
        screen.message = "No legal move available.";
}
// Starts a background search on a copy of the game.
void Game::think(bool hint) {
    Session snapshot = session;
    started = std::chrono::steady_clock::now();
    worker = std::async(std::launch::async,
                        [snapshot = std::move(snapshot), this] { return engine.act(snapshot); });
    screen.busy = true;
    hintJob = hint;
    if (!hint)
        screen.hint = {};
}
bool Game::step() {
    if (menu)
        return menuStep();
    collect();
    bool live = screen.viewing == session.history.size();
    if (!screen.busy && !paused && live && !session.state.winner &&
        computer(screen.mode, session.actor()))
        think(false);
    std::optional<Session> historical;
    if (!live)
        historical.emplace(session.replay(screen.viewing));
    screen.elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    drawGame(session, live ? session : *historical, screen);
    return handleKey(getch(), live);
}
bool Game::handleKey(int ch, bool live) {
    if (ch == 'q' || ch == 'Q')
        return false;
    if (ch == 'v' || ch == 'V')
        screen.explain = !screen.explain;
    if (ch == 'p' || ch == 'P') {
        paused = !paused;
        screen.message = paused ? "AI paused after current move." : "AI resumed.";
    }
    if (LINES < 28 || COLS < 76)
        return true;
    if (ch == KEY_LEFT)
        screen.cx = (screen.cx + Size - 1) % Size;
    if (ch == KEY_RIGHT)
        screen.cx = (screen.cx + 1) % Size;
    if (ch == KEY_UP)
        screen.cy = (screen.cy + Size - 1) % Size;
    if (ch == KEY_DOWN)
        screen.cy = (screen.cy + 1) % Size;
    // Nothing below may change the game while the AI is thinking.
    if (screen.busy)
        return true;
    if (ch == 'n' || ch == 'N') {
        menu = true;
        return true;
    }
    if (ch == '[' && screen.viewing > 0) {
        --screen.viewing;
        screen.hint = {};
        return true;
    }
    if (ch == ']' && screen.viewing < session.history.size()) {
        ++screen.viewing;
        return true;
    }
    if (ch == 's' || ch == 'S') {
        screen.message =
            session.save("gomoku.save") ? "Saved to gomoku.save." : "Cannot save game.";
        return true;
    }
    if (ch == 'l' || ch == 'L') {
        if (session.load("gomoku.save")) {
            screen.viewing = session.history.size();
            screen.hint = {};
            screen.last = {};
            screen.message = "Loaded gomoku.save.";
        } else
            screen.message = "Invalid or missing save; current game kept.";
        return true;
    }
    if (ch == 'u' || ch == 'U') {
        // Against the AI, go back to the previous human turn.
        session.undo();
        while (!session.history.empty() && screen.mode != 2 && screen.mode != 4 &&
               computer(screen.mode, session.actor()))
            session.undo();
        screen.viewing = session.history.size();
        screen.hint = {};
        screen.last = {};
        screen.message = "Move undone.";
        return true;
    }
    // Moves are only for a human, on the live position of an unfinished game.
    if (!live || session.state.winner || computer(screen.mode, session.actor()))
        return true;
    if (ch == 'h' || ch == 'H') {
        think(true);
        return true;
    }
    if (session.needsChoice()) {
        Choice choice = ch == 'b' || ch == 'B'   ? Choice::Black
                        : ch == 'w' || ch == 'W' ? Choice::White
                        : ch == 'e' || ch == 'E' ? Choice::Extend
                                                 : Choice::None;
        if (session.choose(choice)) {
            screen.viewing = session.history.size();
            screen.hint = {};
            screen.last = {};
            screen.message = choice == Choice::Extend ? "Place one White and one Black stone."
                                                      : "Colors assigned; White moves next.";
        }
        return true;
    }
    if (ch == KEY_MOUSE) {
        MEVENT event;
        if (getmouse(&event) == OK && event.x >= 4 && event.x <= 40 && event.y >= 4 &&
            event.y <= 22) {
            screen.cx = (event.x - 4) / 2;
            screen.cy = event.y - 4;
            ch = ' ';
        }
    }
    if (ch == ' ' || ch == '\n' || ch == KEY_ENTER) {
        if (session.play({screen.cx, screen.cy})) {
            screen.viewing = session.history.size();
            screen.hint = {};
            screen.message = "Move played.";
        } else
            screen.message = "Illegal move: occupied square, opening restriction or double-three.";
    }
    return true;
}
// After a failure the game stays usable: hotseat mode on the live position.
void Game::recover(const char *message) {
    // Wait for a running search so that no second one starts beside it.
    if (screen.busy && worker.valid())
        worker.wait();
    screen.message = message;
    screen.mode = 2;
    screen.busy = false;
    screen.viewing = session.history.size();
    screen.explain = false;
}
} // namespace
bool computer(int mode, int player) {
    return mode == 4 || (mode == 1 && player == 2) || (mode == 3 && player == 1);
}
int run() {
    Terminal terminal;
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    Game game;
    while (!stopped) {
        try {
            if (!game.step())
                break;
        } catch (const std::bad_alloc &) {
            game.recover("Not enough memory; try undo or start a new game.");
        } catch (const std::exception &) {
            game.recover("Operation failed; current game remains available.");
        }
    }
    return 0;
}
} // namespace gomoku
