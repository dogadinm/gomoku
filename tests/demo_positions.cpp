// make demo: builds demo/*.save, one position per rule of the subject. Each
// position is reached by real moves, and the move to show at the defense is
// checked against the rules before the file is written.
#include "rules/session.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
using namespace gomoku;
const std::string Dir = "demo/";
void fail(const std::string &what) {
    std::cerr << "FAIL: " << what << '\n';
    std::exit(1);
}
// Far, isolated stones to keep the alternation when one side has nothing to do.
std::vector<Move> fillers[3] = {
    {},
    {{18, 18}, {15, 18}, {12, 18}, {9, 18}, {6, 18}, {3, 18}, {18, 15}, {18, 12}, {18, 9}},
    {{18, 0}, {15, 0}, {12, 0}, {9, 0}, {6, 0}, {3, 0}, {18, 3}, {18, 6}, {0, 18}}};
// Places X stones `x` and O stones `o` in order, Black first, padding with fillers,
// so that `next` is to move at the end.
void extend(Session &s, std::vector<Move> x, std::vector<Move> o, int next) {
    size_t xi = 0, oi = 0;
    auto filler = [&](int p) {
        for (Move m : fillers[p])
            if (s.state.legal(m))
                return m;
        fail("out of filler cells");
        return Move{};
    };
    while (xi < x.size() || oi < o.size() || s.state.turn != next) {
        int p = s.state.turn;
        Move m;
        if (p == 1)
            m = xi < x.size() ? x[xi++] : filler(1);
        else
            m = oi < o.size() ? o[oi++] : filler(2);
        if (!s.play(m))
            fail("setup move " + std::to_string(m.x) + "," + std::to_string(m.y));
    }
}
Session build(std::vector<Move> x, std::vector<Move> o, int next, Rules rules = {}) {
    Session s;
    s.state.rules = rules;
    extend(s, x, o, next);
    return s;
}
void save(const Session &s, const std::string &name) {
    if (!s.save(Dir + name))
        fail("save " + name);
    std::cout << "demo/" << name << " checked\n";
}
int main() {
    // Cells in comments use the board labels: column letter, row number.
    // 1. Capture in all eight directions at once: X at J10 takes 8 pairs.
    {
        std::vector<Move> x, o;
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
                if (dx || dy) {
                    o.push_back({9 + dx, 9 + dy});
                    o.push_back({9 + 2 * dx, 9 + 2 * dy});
                    x.push_back({9 + 3 * dx, 9 + 3 * dy});
                }
        Rules rules;
        rules.doubleThree = false; // the ring of O stones is not the point here
        Session s = build(x, o, 1, rules);
        State t = s.state;
        if (!t.play({9, 9}) || t.captured[1] != 16)
            fail("eight-direction capture");
        save(s, "01-capture-8-directions.save");
    }
    // 2. Moving into a flanked position is safe: O at K10 is not captured.
    {
        Session s = build({{8, 9}, {11, 9}}, {{9, 9}}, 2);
        State t = s.state;
        if (!t.play({10, 9}) || t.captured[1] != 0 || t.at(9, 9) != 2 || t.at(10, 9) != 2)
            fail("move into capture");
        save(s, "02-no-capture-by-moving-in.save");
    }
    // 3. Double three (subject figure): X at E5 is forbidden.
    std::vector<Move> three = {{1, 1}, {2, 2}, {5, 4}, {6, 4}};
    {
        Session s = build(three, {}, 1);
        if (s.state.legal({4, 4}))
            fail("double three must be forbidden");
        save(s, "03-double-three-forbidden.save");
    }
    // 4. Same, but O at D5 closes one three: E5 is legal.
    {
        Session s = build(three, {{3, 4}}, 1);
        if (!s.state.legal({4, 4}))
            fail("blocked double three must be legal");
        save(s, "04-double-three-blocked.save");
    }
    // 5. Same double three, but E5 also captures the O pair E6-E7: allowed.
    {
        std::vector<Move> x = three;
        x.push_back({4, 7});
        Session s = build(x, {{4, 5}, {4, 6}}, 1);
        State t = s.state;
        if (!t.play({4, 4}) || t.captured[1] != 2)
            fail("double three by capture must be legal");
        save(s, "05-double-three-by-capture.save");
    }
    // 6. Five that can be broken: X at J10 makes five, O must capture at H9.
    {
        Session s = build({{5, 9}, {6, 9}, {7, 9}, {8, 9}, {7, 10}}, {{7, 11}}, 1);
        State t = s.state;
        if (!t.play({9, 9}) || t.winner || t.pending != 1)
            fail("breakable five must be pending");
        if (!t.play({7, 8}) || t.winner || t.captured[2] != 2)
            fail("capture must break the five");
        save(s, "06-breakable-five.save");
    }
    // 7. Five or more wins: J10 fills the gap and makes six in a row.
    {
        Session s = build({{4, 9}, {5, 9}, {6, 9}, {8, 9}, {9, 9}}, {}, 1);
        State t = s.state;
        if (!t.play({7, 9}) || t.winner != 1)
            fail("six in a row must win");
        save(s, "07-overline-wins.save");
    }
    // 8. Ten captured stones win: X has taken four pairs; L18 takes the fifth.
    {
        Session s;
        auto play = [&](Move m) {
            if (!s.play(m))
                fail("capture-win setup");
        };
        // Four capture rounds on separate rows: O O pair, X flanks it.
        for (int k = 0; k < 4; ++k) {
            int y = 2 + 4 * k;
            play({2, y});      // X anchor
            play({3, y});      // O
            play({15, y + 1}); // X filler
            play({4, y});      // O pair complete
            play({5, y});      // X captures
            play({16, y + 2}); // O filler
        }
        if (s.state.captured[1] != 8)
            fail("eight captured");
        extend(s, {{8, 17}}, {{9, 17}, {10, 17}}, 1);
        State t = s.state;
        if (!t.play({11, 17}) || t.winner != 1 || t.captured[1] != 10)
            fail("tenth capture wins");
        save(s, "08-ten-captures-win.save");
    }
    // 9. Endgame capture: O has eight captured stones and can take an X pair
    //    (M11-N11, anchor L11). X's five at J18 is not a win: O answers O11,
    //    reaches ten and wins.
    {
        Session s;
        auto play = [&](Move m) {
            if (!s.play(m))
                fail("endgame capture setup");
        };
        for (int k = 0; k < 4; ++k) {
            int y = 1 + 4 * k;
            play({15, y + 1}); // X filler
            play({2, y});      // O anchor
            play({3, y});      // X
            play({17, y + 2}); // O filler
            play({4, y});      // X pair complete
            play({5, y});      // O captures
        }
        if (s.state.captured[2] != 8)
            fail("O eight captured");
        extend(s, {{5, 17}, {6, 17}, {7, 17}, {8, 17}, {12, 10}, {13, 10}}, {{11, 10}}, 1);
        State t = s.state;
        if (!t.play({9, 17}) || t.winner || t.pending != 1)
            fail("five must be pending against a tenth capture");
        if (!t.play({14, 10}) || t.winner != 2 || t.captured[2] != 10)
            fail("tenth capture must win");
        save(s, "09-five-vs-tenth-capture.save");
    }
    return 0;
}
