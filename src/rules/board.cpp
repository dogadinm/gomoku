#include "rules/patterns.hpp"
#include <cstdlib>
namespace gomoku {
const int *Board::cells() {
    return tables().cells.data();
}
Board::Board(const State &s) {
    const Tables &t = tables();
    cell.fill(Wall);
    for (int i : t.cells)
        cell[i] = 0;
    for (int y = 0; y < Size; ++y)
        for (int x = 0; x < Size; ++x) {
            int v = s.board[y * Size + x];
            if (v == 1 || v == 2) {
                int i = index({x, y});
                cell[i] = static_cast<int8_t>(v);
                key ^= t.zobrist[v][i];
                ++stones;
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dx = -2; dx <= 2; ++dx)
                        ++around[i + dy * Width + dx];
            }
        }
    for (int i : t.cells)
        for (int axis = 0; axis < 4; ++axis) {
            unsigned value = 0;
            for (int k = 0; k < 8; ++k)
                value |= unsigned(cell[i + offsetOf(k) * Step[axis]]) << (2 * k);
            code[i][axis] = static_cast<uint16_t>(value);
        }
    for (int i : t.cells)
        for (int axis = 0; axis < 4; ++axis)
            summarize(i, axis);
    captured = s.captured;
    turn = s.turn;
    winner = s.winner;
    pending = s.pending;
    moves = s.moves;
    rules = s.rules;
}
const LineInfo &Board::line(int i, int axis, int player) const {
    return tables().info[player - 1][code[i][axis]];
}
void Board::summarize(int i, int axis) {
    static constexpr int Digit[4] = {1, ShapeCount, ShapeCount * ShapeCount,
                                     ShapeCount * ShapeCount * ShapeCount};
    const Tables &t = tables();
    for (int p : {1, 2}) {
        // Only this axis changed: adjust the totals by the difference.
        CellSummary &c = summaries[i][p - 1];
        const LineInfo &a = t.info[p - 1][code[i][axis]];
        c.shapes = static_cast<uint16_t>(c.shapes + (a.shape - c.shape[axis]) * Digit[axis]);
        c.pairs = static_cast<uint8_t>(c.pairs + a.pairs - c.axisPairs[axis]);
        c.freeThrees = static_cast<uint8_t>(c.freeThrees + (a.flags & FreeThreeFlag) -
                                            (c.axisFlags[axis] & FreeThreeFlag));
        c.shape[axis] = a.shape;
        c.axisPairs[axis] = a.pairs;
        c.axisFlags[axis] = a.flags;
        c.level = t.levels[c.shapes];
    }
}
bool Board::legalFor(int i, int player) const {
    if (winner || cell[i] != 0)
        return false;
    if (rules.opening == Opening::Pro && (moves == 0 || moves == 2)) {
        Move m = move(i);
        if (moves == 0 && !(m == Move{9, 9}))
            return false;
        if (moves == 2 && std::abs(m.x - 9) <= 2 && std::abs(m.y - 9) <= 2)
            return false;
    }
    if (!rules.doubleThree)
        return true;
    const CellSummary &c = summary(i, player);
    // Capturing moves may create a double three (subject appendix).
    return (rules.captures && c.pairs) || c.freeThrees < 2;
}
int Board::capturePairs(int i, int player) const {
    return rules.captures ? summary(i, player).pairs : 0;
}
bool Board::fiveAt(int i, int player) const {
    // Summaries are kept for empty cells only; read the codes directly.
    for (int axis = 0; axis < 4; ++axis)
        if (line(i, axis, player).shape == Five)
            return true;
    return false;
}
bool Board::hasFive(int player) const {
    for (int i : tables().cells)
        if (cell[i] == player && fiveAt(i, player))
            return true;
    return false;
}
void Board::put(int i, int value) {
    int old = cell[i];
    const Tables &t = tables();
    cell[i] = static_cast<int8_t>(value);
    key ^= t.zobrist[old == Wall ? 0 : old][i] ^ t.zobrist[value][i];
    for (int axis = 0; axis < 4; ++axis)
        for (int k = 1; k <= 4; ++k) {
            // Seen from i + k*step this cell is neighbor offset -k, and from
            // i - k*step it is offset +k.
            int after = 2 * (4 - k), before = 2 * (k + 3);
            uint16_t &a = code[i + k * Step[axis]][axis];
            a = static_cast<uint16_t>((a & ~(3u << after)) | unsigned(value) << after);
            uint16_t &b = code[i - k * Step[axis]][axis];
            b = static_cast<uint16_t>((b & ~(3u << before)) | unsigned(value) << before);
            // Only empty cells need a current summary; an occupied one is
            // rebuilt when it is emptied again.
            if (cell[i + k * Step[axis]] == 0)
                summarize(i + k * Step[axis], axis);
            if (cell[i - k * Step[axis]] == 0)
                summarize(i - k * Step[axis], axis);
        }
    if (value == 0)
        for (int axis = 0; axis < 4; ++axis)
            summarize(i, axis);
    if ((old == 0) != (value == 0)) {
        int delta = value ? 1 : -1;
        stones += delta;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
                around[i + dy * Width + dx] = static_cast<uint8_t>(around[i + dy * Width + dx] + delta);
    }
}
bool Board::captureDefense(int player) {
    int enemy = 3 - player;
    // Mirror of State::captureDefense: can the enemy break the five (or reach
    // ten captured stones) with one capture?
    for (int i : tables().cells) {
        if (cell[i] != 0 || !capturePairs(i, enemy))
            continue;
        int removed[16], count = 0;
        put(i, enemy);
        for (int axis = 0; axis < 4; ++axis)
            for (int sign : {-1, 1}) {
                int d = sign * Step[axis];
                if (cell[i + d] == player && cell[i + 2 * d] == player && cell[i + 3 * d] == enemy) {
                    removed[count++] = i + d;
                    removed[count++] = i + 2 * d;
                }
            }
        for (int k = 0; k < count; ++k)
            put(removed[k], 0);
        bool defends = captured[enemy] + count >= 10 || !hasFive(player);
        for (int k = 0; k < count; ++k)
            put(removed[k], player);
        put(i, 0);
        if (defends)
            return true;
    }
    return false;
}
void Board::make(int i, Undo &undo) {
    int p = turn, o = 3 - p;
    undo.cell = i;
    undo.count = 0;
    undo.captured = captured[p];
    undo.winner = winner;
    undo.pending = pending;
    put(i, p);
    if (rules.captures)
        for (int axis = 0; axis < 4; ++axis)
            for (int sign : {-1, 1}) {
                int d = sign * Step[axis];
                if (cell[i + d] == o && cell[i + 2 * d] == o && cell[i + 3 * d] == p) {
                    undo.removed[undo.count++] = i + d;
                    undo.removed[undo.count++] = i + 2 * d;
                }
            }
    for (int k = 0; k < undo.count; ++k)
        put(undo.removed[k], 0);
    captured[p] += undo.count;
    ++moves;
    // Same order of win checks as State::applyLegal.
    if (captured[p] >= 10)
        winner = p;
    else if (pending && hasFive(pending))
        winner = pending;
    else if (fiveAt(i, p)) {
        if (!rules.captures || !captureDefense(p))
            winner = p;
        else
            pending = p;
    } else
        pending = 0;
    turn = o;
    if (!winner && stones == Cells)
        winner = 3;
}
void Board::unmake(const Undo &undo) {
    turn = 3 - turn;
    for (int k = 0; k < undo.count; ++k)
        put(undo.removed[k], 3 - turn);
    put(undo.cell, 0);
    captured[turn] = undo.captured;
    winner = undo.winner;
    pending = undo.pending;
    --moves;
}
uint64_t Board::hash() const {
    const Tables &t = tables();
    uint64_t h = key ^ t.pending[pending] ^ t.captured[1][std::min(captured[1], 31)] ^
                 t.captured[2][std::min(captured[2], 31)];
    if (turn == 2)
        h ^= t.turn;
    h ^= t.rules[rules.captures * 4 + rules.doubleThree * 2];
    if (rules.opening == Opening::Pro && moves < 3)
        h ^= t.rules[1 + moves * 2];
    return h;
}
State Board::state() const {
    State s;
    for (int i = 0; i < Cells; ++i)
        s.board[i] = cell[tables().cells[i]];
    s.captured = captured;
    s.turn = turn;
    s.winner = winner;
    s.pending = pending;
    s.moves = moves;
    s.rules = rules;
    return s;
}
} // namespace gomoku
