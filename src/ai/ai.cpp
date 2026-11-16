#include "ai/search.hpp"
#include <new>
#include <thread>
#include <vector>
namespace gomoku {
namespace {
constexpr int VcfBudget = 4000, VcfDepth = 10, MaxThreads = 4;
} // namespace
SearchResult AI::search(const State &s, int milliseconds, int maxDepth,
                        const HistoryProfile &recent) {
    using Clock = std::chrono::steady_clock;
    auto start = Clock::now();
    milliseconds = std::max(1, milliseconds);
    // Up to 472 ms to finish depth 10; every move stays under 0.5 s.
    auto target = start + std::chrono::milliseconds(milliseconds);
    deadline = target + std::chrono::milliseconds(milliseconds / 8);
    profile = recent;
    stop = false;
    SearchResult result;
    bool fresh = false;
    if (!table) {
        // Kept between moves; smaller or none when memory is short.
        for (size_t size : {size_t(1) << 20, size_t(1) << 16}) {
            table.reset(new (std::nothrow) std::atomic<uint64_t>[2 * size]);
            if (table) {
                entries = size;
                fresh = true;
                break;
            }
        }
        result.lowMemory = !table;
    }
    // Only the current generation may cut off; clear when the 6-bit counter wraps.
    generation = (generation + 1) & 63;
    if (table && (fresh || generation == 0))
        for (size_t k = 0; k < 2 * entries; ++k)
            table[k].store(0, std::memory_order_relaxed);
    if (s.winner)
        return result;
    std::unique_ptr<Worker> main(new (std::nothrow) Worker(*this, s));
    Candidates initial;
    if (main)
        main->generate(initial, Normal, -1, 0, MaxPly);
    if (initial.empty()) {
        result.lowMemory |= !main;
        auto fallback = s.candidates();
        if (!fallback.empty())
            result.move = fallback.front();
        return result;
    }
    Worker &w = *main;
    result.move = Board::move(pick(initial, 0).cell);
    std::vector<std::thread> helpers;
    std::atomic<uint64_t> helperNodes{0};
    int depthLimit = std::clamp(maxDepth, 1, MaxPly - 1);
    try {
        // A forced win by fours (VCF) ends the search.
        long budget = VcfBudget;
        if (w.vcf(s.turn, VcfDepth, budget, 0) && w.rootBest >= 0) {
            result.move = Board::move(w.rootBest);
            result.score = Win - 1;
            result.threat = 1;
            result.depth = 1;
        } else {
            // Lazy SMP: helpers search the same root and share the table.
            unsigned cores = std::thread::hardware_concurrency();
            int count = std::clamp(static_cast<int>(cores), 1, MaxThreads) - 1;
            for (int id = 1; id <= count && table; ++id) {
                try {
                    helpers.emplace_back([this, &s, &helperNodes, id, depthLimit] {
                        std::unique_ptr<Worker> helper(new (std::nothrow) Worker(*this, s));
                        if (!helper)
                            return;
                        try {
                            for (int depth = 1 + id % 2; depth <= depthLimit; ++depth)
                                helper->negamax(depth, -Infinity, Infinity, 0);
                        } catch (const Timeout &) {
                        }
                        helperNodes += helper->nodes;
                    });
                } catch (...) {
                    break;
                }
            }
            result.threads = 1 + static_cast<int>(helpers.size());
            Clock::duration previous{}, last{};
            for (int depth = 1; depth <= depthLimit; ++depth) {
                auto began = Clock::now();
                w.rootBest = -1;
                int score = w.negamax(depth, -Infinity, Infinity, 0);
                if (w.rootBest >= 0)
                    result.move = Board::move(w.rootBest);
                result.score = score;
                result.depth = depth;
                // Only a win now is exact; longer mates are rechecked up to depth 10.
                if (score >= Win - 1 || (std::abs(score) >= WinBound && depth >= MinimumDepth))
                    break;
                previous = last;
                last = Clock::now() - began;
                // Do not start an iteration predicted to exceed the target.
                if (depth >= MinimumDepth) {
                    double growth = previous.count() > 0
                                        ? std::clamp(double(last.count()) / previous.count(), 1.5, 4.0)
                                        : 3.0;
                    if (Clock::now() + std::chrono::duration_cast<Clock::duration>(last * growth) >
                        target)
                        break;
                }
            }
        }
    } catch (const Timeout &) {
        // Last completed iteration, or a better root move already searched.
        if (w.rootBest >= 0 && result.depth > 0)
            result.move = Board::move(w.rootBest);
    }
    stop = true;
    for (auto &helper : helpers)
        helper.join();
    // Principal variation from the transposition table.
    Board pv(s);
    int cell = Board::index(result.move);
    Entry entry;
    while (cell >= 0 && pv.legal(cell) && result.principal.size() < 16 && !pv.winner) {
        result.principal.push_back(Board::move(cell));
        Board::Undo undo;
        pv.make(cell, undo);
        cell = probe(pv.hash(), entry) ? entry.move : -1;
    }
    result.nodes = w.nodes + helperNodes;
    result.hits = w.hits;
    result.selective = w.selective;
    result.milliseconds =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    return result;
}
SearchResult AI::act(const Session &session, int milliseconds) {
    auto start = std::chrono::steady_clock::now();
    HistoryProfile recent;
    bool lowMemory = false;
    try {
        recent = historyProfile(session);
    } catch (const std::bad_alloc &) {
        lowMemory = true;
    }
    int remaining = std::max(
        1, milliseconds - static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now() - start)
                                               .count()));
    if (!session.openingActive()) {
        auto result = search(session.state, remaining, 32, recent);
        result.lowMemory |= lowMemory;
        result.milliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        return result;
    }
    SearchResult result;
    if (session.needsChoice()) {
        // Both choices are available: take the color favored by a normal search.
        result = search(session.state, remaining, 32, recent);
        int favored = result.score >= 0 ? session.state.turn : 3 - session.state.turn;
        result.choice = favored == 1 ? Choice::Black : Choice::White;
        result.move = {};
        result.principal.clear();
    } else {
        // The opponent may choose either color: propose a balanced setup.
        int best = Infinity;
        for (Move move : session.state.candidates()) {
            State next = session.state;
            // Setup stones never capture: place directly.
            next.board[move.y * Size + move.x] = static_cast<int8_t>(next.turn);
            ++next.moves;
            next.turn = 3 - next.turn;
            int score = std::abs(evaluate(next));
            int distance = std::abs(move.x - 9) + std::abs(move.y - 9);
            int rank = score * 100 + distance;
            if (rank < best) {
                best = rank;
                result.move = move;
                result.score = score;
            }
            ++result.nodes;
            if (std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(milliseconds))
                break;
        }
    }
    result.lowMemory |= lowMemory;
    result.milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result;
}
} // namespace gomoku
