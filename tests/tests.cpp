#include "ai.hpp"
#include "game.hpp"
#include "session.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace gomoku;
int checks = 0;
void require(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
void put(State &s, int x, int y, int p) {
    s.board[y * Size + x] = static_cast<int8_t>(p);
}
void rules() {
    State s;
    require(!s.legal({-1, 0}) && !s.legal({19, 19}), "bounds");
    require(s.play({9, 9}) && !s.play({9, 9}), "occupied square");
    s = State{};
    put(s, 3, 0, 1);
    put(s, 1, 0, 2);
    put(s, 2, 0, 2);
    require(s.play({0, 0}) && s.captured[1] == 2 && s.at(1, 0) == 0 && s.at(2, 0) == 0,
            "horizontal capture");
    s = State{};
    put(s, 3, 3, 1);
    put(s, 1, 1, 2);
    put(s, 2, 2, 2);
    require(s.play({0, 0}) && s.captured[1] == 2, "diagonal capture");
    s = State{};
    put(s, 6, 9, 1);
    put(s, 7, 9, 2);
    put(s, 8, 9, 2);
    put(s, 12, 9, 1);
    put(s, 10, 9, 2);
    put(s, 11, 9, 2);
    require(s.play({9, 9}) && s.captured[1] == 4, "simultaneous captures");
    s = State{};
    put(s, 0, 0, 2);
    put(s, 1, 0, 1);
    put(s, 3, 0, 2);
    require(s.play({2, 0}) && s.at(1, 0) == 1 && s.at(2, 0) == 1, "no passive capture");
    s = State{};
    put(s, 4, 0, 1);
    put(s, 1, 0, 2);
    put(s, 2, 0, 2);
    put(s, 3, 0, 2);
    require(s.play({0, 0}) && s.captured[1] == 0, "cannot capture triple");
    s = State{};
    put(s, 8, 9, 1);
    put(s, 10, 9, 1);
    put(s, 9, 8, 1);
    put(s, 9, 10, 1);
    require(!s.legal({9, 9}), "straight double three");
    s.rules.doubleThree = false;
    require(s.legal({9, 9}), "optional double three rule");
    s.rules.doubleThree = true;
    put(s, 7, 9, 2);
    require(s.legal({9, 9}), "blocked three");
    s = State{};
    put(s, 7, 9, 1);
    put(s, 10, 9, 1);
    put(s, 9, 7, 1);
    put(s, 9, 10, 1);
    require(!s.legal({9, 9}), "broken double three");
    s = State{};
    put(s, 8, 9, 1);
    put(s, 10, 9, 1);
    put(s, 9, 8, 1);
    put(s, 9, 10, 1);
    put(s, 10, 10, 2);
    put(s, 11, 11, 2);
    put(s, 12, 12, 1);
    require(s.play({9, 9}) && s.captured[1] == 2, "capture exempts double three");
    s = State{};
    for (int x = 0; x < 4; ++x)
        put(s, x, 0, 1);
    require(s.play({4, 0}) && s.winner == 1, "five on edge");
    require(!s.play({5, 0}), "game ends");
    s = State{};
    for (int x : {0, 1, 2, 4, 5})
        put(s, x, 0, 1);
    require(s.play({3, 0}) && s.winner == 1, "overline wins");
    s = State{};
    s.captured[1] = 8;
    put(s, 3, 0, 1);
    put(s, 1, 0, 2);
    put(s, 2, 0, 2);
    require(s.play({0, 0}) && s.winner == 1, "ten captures win");
    // White can break the horizontal five with a vertical capture.
    s = State{};
    for (int x = 5; x < 9; ++x)
        put(s, x, 9, 1);
    put(s, 7, 10, 1);
    put(s, 7, 11, 2);
    require(s.play({9, 9}) && !s.winner && s.pending == 1, "vulnerable five waits");
    State defended = s;
    require(defended.play({7, 8}) && !defended.winner && !defended.pending, "capture breaks five");
    require(s.play({0, 0}) && s.winner == 1, "ignoring five loses");
    s = State{};
    for (int x = 0; x < 4; ++x)
        put(s, x, 0, 1);
    s.captured[2] = 8;
    put(s, 8, 10, 1);
    put(s, 9, 10, 1);
    put(s, 10, 10, 2);
    require(s.play({4, 0}) && !s.winner, "capture victory can answer five elsewhere");
    require(s.play({7, 10}) && s.winner == 2, "capture victory takes precedence");
    s = State{};
    s.rules.captures = false;
    put(s, 3, 0, 1);
    put(s, 1, 0, 2);
    put(s, 2, 0, 2);
    require(s.play({0, 0}) && s.captured[1] == 0, "captures disabled");
}
void symmetries() {
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (!dx && !dy)
                continue;
            State s;
            put(s, 9 + dx, 9 + dy, 2);
            put(s, 9 + 2 * dx, 9 + 2 * dy, 2);
            put(s, 9 + 3 * dx, 9 + 3 * dy, 1);
            require(s.play({9, 9}) && s.captured[1] == 2, "capture in every direction");
            s = State{};
            for (int k = 1; k < 5; ++k)
                put(s, 9 + k * dx, 9 + k * dy, 1);
            require(s.play({9, 9}) && s.winner == 1, "five in every direction");
        }
    }
    State edge;
    put(edge, 0, 0, 1);
    put(edge, 2, 0, 1);
    put(edge, 1, 1, 1);
    put(edge, 1, 2, 1);
    require(edge.legal({1, 0}), "board boundary closes an apparent three");
    State s;
    for (int x = 5; x < 9; ++x) {
        put(s, x, 9, 1);
        put(s, x, 12, 1);
    }
    put(s, 9, 12, 1);
    put(s, 7, 10, 1);
    put(s, 7, 11, 2);
    require(s.play({9, 9}) && s.winner == 1, "breaking only one of two fives cannot defend");
}
void persistence() {
    Session s;
    require(s.play({9, 9}) && s.play({10, 10}), "session moves");
    auto hash = s.state.hash();
    require(s.save("build/test.save"), "save");
    Session loaded;
    require(loaded.load("build/test.save") && loaded.state.hash() == hash, "load roundtrip");
    loaded.undo();
    require(loaded.history.size() == 1 && loaded.state.turn == 2 && loaded.state.at(10, 10) == 0,
            "undo");
    require(loaded.position(0).moves == 0, "replay start");
    std::ofstream bad("build/test.save");
    bad << "GOMOKU 1\n1 1 2\n9 9\n9 9\n";
    bad.close();
    require(!s.load("build/test.save") && s.state.hash() == hash, "invalid load is atomic");
    std::remove("build/test.save");
}
void search() {
    AI ai;
    State s;
    for (int x = 0; x < 4; ++x)
        put(s, x, 0, 1);
    auto result = ai.search(s, 100);
    require(result.move == Move{4, 0}, "AI takes win");
    s = State{};
    for (int x = 0; x < 4; ++x)
        put(s, x, 0, 2);
    result = ai.search(s, 200);
    require(result.move == Move{4, 0}, "AI blocks forced win");
    s = State{};
    s.captured[1] = 8;
    put(s, 3, 0, 1);
    put(s, 1, 0, 2);
    put(s, 2, 0, 2);
    result = ai.search(s, 100);
    require(result.move == Move{0, 0}, "AI takes capture win");
    s = State{};
    for (int x = 5; x < 9; ++x)
        put(s, x, 9, 1);
    put(s, 7, 10, 1);
    put(s, 7, 11, 2);
    require(s.play({9, 9}) && s.pending == 1, "pending five AI fixture");
    result = ai.search(s, 200);
    require(result.move == Move{7, 8}, "AI captures to defend pending five");
    s = State{};
    result = ai.search(s, 1);
    require(s.legal(result.move), "timeout returns legal fallback");
    s.winner = 1;
    result = ai.search(s, 100);
    require(result.move.x == -1, "terminal search has no move");
    s = State{};
    auto hash = s.hash();
    ai.search(s, 10);
    require(hash == s.hash(), "search leaves state unchanged");
}
void openings() {
    State pro;
    pro.rules.opening = Opening::Pro;
    require(!pro.legal({8, 9}) && pro.play({9, 9}), "Pro begins at center");
    require(pro.play({10, 9}), "Pro white unrestricted");
    require(!pro.legal({11, 11}) && !pro.legal({7, 7}), "Pro excludes central 5x5");
    require(pro.play({12, 9}) && pro.play({9, 10}) && pro.legal({9, 8}),
            "Pro restrictions end after third move");
    AI ai;
    State p;
    p.rules.opening = Opening::Pro;
    p.play({9, 9});
    p.play({10, 9});
    auto suggestion = ai.search(p, 20);
    require(p.legal(suggestion.move), "AI respects Pro");
    for (Opening opening : {Opening::Swap, Opening::Swap2}) {
        Session base;
        base.state.rules.opening = opening;
        require(base.actor() == 1 && !base.choose(Choice::Black), "no premature color choice");
        require(base.play({9, 9}) && base.actor() == 1, "same player sets white");
        require(base.play({10, 9}) && base.play({9, 10}), "three setup stones");
        require(base.needsChoice() && base.actor() == 2 && !base.play({8, 8}),
                "choice blocks placement");
        for (Choice choice : {Choice::Black, Choice::White}) {
            Session game = base;
            require(game.choose(choice) && !game.openingActive() && game.state.turn == 2,
                    "assign colors then white moves");
            require(game.actor() == (choice == Choice::White ? 2 : 1),
                    "color ownership maps to player");
            require(!game.choose(choice), "cannot choose twice");
            require(game.save("build/opening.save"), "save color choice");
            Session restored;
            require(restored.load("build/opening.save") && restored.actor() == game.actor() &&
                        restored.blackPlayer == game.blackPlayer,
                    "restore color ownership");
            restored.undo();
            require(restored.needsChoice() && restored.actor() == 2, "undo color choice");
        }
        auto result = ai.act(base, 20);
        require(result.choice == Choice::Black || result.choice == Choice::White,
                "AI selects a color");
        if (opening == Opening::Swap)
            require(!base.choose(Choice::Extend), "Swap cannot extend");
        else {
            require(base.choose(Choice::Extend) && base.actor() == 2 && !base.needsChoice(),
                    "Swap2 extends setup");
            require(base.play({8, 9}) && base.actor() == 2 && base.play({10, 10}),
                    "P2 places white and black");
            require(base.needsChoice() && base.actor() == 1 && !base.choose(Choice::Extend),
                    "final choice belongs to P1");
            require(base.save("build/opening.save"), "save pending final choice");
            Session restored;
            require(restored.load("build/opening.save") && restored.extended &&
                        restored.needsChoice() && restored.actor() == 1,
                    "restore final choice");
            require(base.choose(Choice::White) && base.actor() == 1 && base.blackPlayer == 2,
                    "P1 chooses white");
            require(base.play({8, 10}) && base.actor() == 2, "normal play after Swap2");
            base.undo();
            base.undo();
            base.undo();
            base.undo();
            base.undo();
            require(base.needsChoice() && !base.extended && base.state.moves == 3,
                    "undo entire extension");
        }
        Session setup;
        setup.state.rules.opening = opening;
        for (int n = 0; n < 3; ++n) {
            auto move = ai.act(setup, 20);
            require(setup.play(move.move), "AI setup is legal");
        }
        require(setup.needsChoice(), "AI reaches color choice");
    }
    Session setup;
    setup.state.rules.opening = Opening::Swap2;
    setup.play({9, 9});
    setup.play({8, 9});
    setup.play({10, 9});
    setup.choose(Choice::Extend);
    require(setup.play({11, 9}) && setup.state.captured[2] == 0 && setup.state.at(9, 9) == 1,
            "setup does not capture stones");
    std::ofstream old("build/opening.save");
    old << "GOMOKU 1\n1 1 1\n9 9\n";
    old.close();
    require(setup.load("build/opening.save") && setup.state.rules.opening == Opening::Standard,
            "load legacy save");
    auto hash = setup.state.hash();
    std::ofstream bad("build/opening.save");
    bad << "GOMOKU 2\n1 1 2 1\nC 1\n";
    bad.close();
    require(!setup.load("build/opening.save") && setup.state.hash() == hash,
            "reject out of phase choice atomically");
    std::remove("build/opening.save");
}
void randomGames() {
    std::mt19937 generator(42);
    for (int g = 0; g < 8; ++g) {
        Session session;
        for (int i = 0; i < 150 && !session.state.winner; ++i) {
            auto moves = session.state.candidates();
            if (moves.empty())
                break;
            auto before = session.state.hash();
            Move move = moves[generator() % moves.size()];
            require(session.play(move), "generated move legal");
            require(session.state.captured[1] % 2 == 0 && session.state.captured[2] % 2 == 0,
                    "capture parity");
            session.undo();
            require(session.state.hash() == before, "undo restores complete position");
            require(session.play(move), "reapply move");
        }
    }
}
int main() {
    try {
        rules();
        symmetries();
        persistence();
        search();
        openings();
        randomGames();
        std::cout << checks << " checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
