#pragma once
#include "game.hpp"
#include <string>
namespace gomoku {
struct Session {
    State state;
    std::vector<Move> history;
    bool play(Move m);
    void undo();
    State position(size_t count) const;
    bool save(const std::string &path) const;
    bool load(const std::string &path);
};
} // namespace gomoku
