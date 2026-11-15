#include "ai.hpp"
#include "fault.hpp"
#include <cstdio>
#include <iostream>
#include <stdexcept>
using namespace gomoku;
int checks = 0;
void require(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> bool rejected(F operation, long n = 0) {
    bool failed = false;
    fault::arm(n);
    try {
        operation();
    } catch (const std::bad_alloc &) {
        failed = true;
    } catch (...) {
        fault::disable();
        throw;
    }
    fault::disable();
    return failed;
}
int main() {
    try {
        Session empty;
        require(rejected([&] { empty.play({9, 9}); }), "move allocation is rejected");
        require(empty.history.empty() && empty.state.moves == 0 && empty.state.at(9, 9) == 0,
                "failed move is atomic");
        Session session;
        session.play({9, 9});
        session.play({10, 9});
        session.play({8, 8});
        auto hash = session.state.hash();
        auto count = session.history.size();
        require(rejected([&] { session.undo(); }), "undo allocation is rejected");
        require(session.history.size() == count && session.state.hash() == hash,
                "failed undo is atomic");
        const std::string path = "build/memory-test.save";
        require(session.save(path), "prepare save");
        int failures = 0, successes = 0;
        for (int n = 0; n < 24; ++n) {
            Session target;
            target.play({0, 0});
            auto before = target.state.hash();
            bool loaded = false;
            bool failed = rejected([&] { loaded = target.load(path); }, n);
            if (failed || !loaded) {
                ++failures;
                require(target.history.size() == 1 && target.state.hash() == before,
                        "failed load preserves current game");
            } else {
                ++successes;
                require(target.state.hash() == hash, "successful load after fault budget");
            }
        }
        require(failures > 0 && successes > 0, "load exercised both failure and recovery");
        std::remove(path.c_str());
        AI engine;
        SearchResult result;
        fault::arm(0);
        try {
            result = engine.act(session, 40);
        } catch (...) {
            fault::disable();
            throw;
        }
        long rejectedCount = fault::failures();
        fault::disable();
        require(rejectedCount >= 2 && result.lowMemory,
                "history and table allocation failures reported");
        require(session.state.legal(result.move), "memory fallback supplies legal move");
        result = engine.act(session, 40);
        require(!result.lowMemory && session.state.legal(result.move),
                "AI recovers after memory becomes available");
        for (Opening opening : {Opening::Swap, Opening::Swap2}) {
            Session setup;
            setup.state.rules.opening = opening;
            setup.play({9, 9});
            AI openingAI;
            fault::arm(0);
            try {
                result = openingAI.act(setup, 20);
            } catch (...) {
                fault::disable();
                throw;
            }
            fault::disable();
            require(setup.state.legal(result.move), "opening setup survives allocation failure");
        }
        std::cout << checks << " memory failure checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        fault::disable();
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
