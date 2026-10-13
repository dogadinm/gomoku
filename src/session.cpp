#include "session.hpp"
#include <cstdio>
#include <fstream>
namespace gomoku {
bool Session::openingActive() const {
    return (state.rules.opening == Opening::Swap || state.rules.opening == Opening::Swap2) &&
           blackPlayer == 0;
}
bool Session::needsChoice() const {
    return openingActive() && (state.moves == (extended ? 5 : 3));
}
int Session::actor() const {
    if (openingActive()) {
        if (needsChoice())
            return extended ? 1 : 2;
        return extended ? 2 : 1;
    }
    int black = blackPlayer ? blackPlayer : 1;
    return state.turn == 1 ? black : 3 - black;
}
bool Session::play(Move m) {
    if (needsChoice() || !state.legal(m))
        return false;
    history.push_back({m, Choice::None});
    if (openingActive()) {
        // Opening stones form a setup, not capture-producing game turns.
        state.board[m.y * Size + m.x] = static_cast<int8_t>(state.turn);
        ++state.moves;
        state.turn = 3 - state.turn;
    } else if (!state.play(m)) {
        history.pop_back();
        return false;
    }
    return true;
}
bool Session::choose(Choice choice) {
    if (!needsChoice() || choice == Choice::None)
        return false;
    if (choice == Choice::Extend && (extended || state.rules.opening != Opening::Swap2))
        return false;
    if (choice != Choice::Black && choice != Choice::White && choice != Choice::Extend)
        return false;
    int player = actor();
    history.push_back({{}, choice});
    if (choice == Choice::Extend)
        extended = true;
    else
        blackPlayer = choice == Choice::Black ? player : 3 - player;
    return true;
}
bool Session::apply(Action action) {
    return action.choice == Choice::None ? play(action.move) : choose(action.choice);
}
Session Session::replay(size_t count) const {
    Session s;
    s.state.rules = state.rules;
    for (size_t i = 0; i < count && i < history.size(); ++i)
        s.apply(history[i]);
    return s;
}
State Session::position(size_t count) const {
    return replay(count).state;
}
void Session::undo() {
    if (!history.empty()) {
        // Build first: allocation failure must not leave a half-undone game.
        Session previous = replay(history.size() - 1);
        *this = std::move(previous);
    }
}
bool Session::save(const std::string &path) const {
    std::string temporary = path + ".tmp";
    std::ofstream file(temporary);
    if (!file)
        return false;
    file << "GOMOKU 2\n"
         << state.rules.captures << ' ' << state.rules.doubleThree << ' '
         << static_cast<int>(state.rules.opening) << ' ' << history.size() << '\n';
    for (Action a : history) {
        if (a.choice == Choice::None)
            file << "M " << a.move.x << ' ' << a.move.y << '\n';
        else
            file << "C " << static_cast<int>(a.choice) << '\n';
    }
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
    int version, captures, threes, opening = 0, count;
    if (!(file >> magic >> version) || magic != "GOMOKU" || (version != 1 && version != 2))
        return false;
    if (!(file >> captures >> threes))
        return false;
    if (version == 2 && !(file >> opening))
        return false;
    if (!(file >> count) || captures < 0 || captures > 1 || threes < 0 || threes > 1 ||
        opening < 0 || opening > 3 || count < 0 || count > 100000)
        return false;
    Session replacement;
    replacement.state.rules = {captures != 0, threes != 0, static_cast<Opening>(opening)};
    for (int i = 0; i < count; ++i) {
        char kind = 'M';
        if (version == 2 && !(file >> kind))
            return false;
        if (kind == 'M') {
            Move m;
            if (!(file >> m.x >> m.y) || !replacement.play(m))
                return false;
        } else if (kind == 'C') {
            int c;
            if (!(file >> c) || c < 1 || c > 3 || !replacement.choose(static_cast<Choice>(c)))
                return false;
        } else
            return false;
    }
    std::string extra;
    if (file >> extra)
        return false;
    *this = std::move(replacement);
    return true;
}
} // namespace gomoku
