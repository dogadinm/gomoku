#include "session.hpp"
#include <cstdio>
#include <fstream>
namespace gomoku {
bool Session::play(Move m) {
    if (!state.legal(m))
        return false;
    history.push_back(m); // Allocate before changing state, so allocation failure is safe.
    if (!state.play(m)) {
        history.pop_back();
        return false;
    }
    return true;
}
State Session::position(size_t count) const {
    State s;
    s.rules = state.rules;
    for (size_t i = 0; i < count && i < history.size(); ++i)
        s.play(history[i]);
    return s;
}
void Session::undo() {
    if (!history.empty()) {
        history.pop_back();
        state = position(history.size());
    }
}
bool Session::save(const std::string &path) const {
    std::string temporary = path + ".tmp";
    std::ofstream file(temporary);
    if (!file)
        return false;
    file << "GOMOKU 1\n"
         << state.rules.captures << ' ' << state.rules.doubleThree << ' ' << history.size() << '\n';
    for (Move m : history)
        file << m.x << ' ' << m.y << '\n';
    file.close();
    if (!file) {
        std::remove(temporary.c_str());
        return false;
    }
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    return true;
}
bool Session::load(const std::string &path) {
    std::ifstream file(path);
    std::string magic;
    int version, captures, threes, count;
    if (!(file >> magic >> version) || magic != "GOMOKU" || version != 1)
        return false;
    if (!(file >> captures >> threes >> count) || captures < 0 || captures > 1 || threes < 0 ||
        threes > 1 || count < 0 || count > 100000)
        return false;
    Session replacement;
    replacement.state.rules = {captures != 0, threes != 0};
    for (int i = 0; i < count; ++i) {
        Move m;
        if (!(file >> m.x >> m.y) || !replacement.play(m))
            return false;
    }
    std::string extra;
    if (file >> extra)
        return false;
    *this = std::move(replacement);
    return true;
}
} // namespace gomoku
