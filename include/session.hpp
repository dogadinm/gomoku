#pragma once
#include "game.hpp"
#include <string>
namespace gomoku {
enum class Choice { None, Black, White, Extend };
struct Action {
    Move move{};
    Choice choice = Choice::None;
};
struct Session {
    State state;
    std::vector<Action> history;
    // Player identities stay fixed while ownership of colors can change.
    int blackPlayer = 0;
    bool extended = false;
    bool openingActive() const;
    bool needsChoice() const;
    int actor() const;
    bool play(Move m);
    bool choose(Choice choice);
    bool apply(Action action);
    void undo();
    Session replay(size_t count) const;
    State position(size_t count) const;
    bool save(const std::string &path) const;
    bool load(const std::string &path);
};
} // namespace gomoku
