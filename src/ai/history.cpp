#include "ai/ai.hpp"
#include <algorithm>
#include <cstdlib>
namespace gomoku {
// Dynamic part: per color, repeated axes and captures in the last 12 real actions.
HistoryProfile AI::historyProfile(const Session &session) {
    HistoryProfile profile;
    size_t begin = session.history.size() > 12 ? session.history.size() - 12 : 0;
    Session past = session.replay(begin);
    std::array<Move, 3> previous{};
    for (size_t i = begin; i < session.history.size(); ++i) {
        const Action &action = session.history[i];
        int p = past.state.turn;
        bool setup = past.openingActive();
        int captured = past.state.captured[p];
        if (!past.apply(action))
            break;
        if (action.choice != Choice::None || setup)
            continue;
        // Recent actions carry greater weight (1..4).
        int weight = 1 + static_cast<int>((i - begin) / 3);
        profile.captures[p] =
            std::min(8, profile.captures[p] + (past.state.captured[p] - captured) / 2 * weight);
        Move old = previous[p];
        if (old.x >= 0) {
            int dx = action.move.x - old.x, dy = action.move.y - old.y;
            if (std::max(std::abs(dx), std::abs(dy)) <= 4) {
                int axis = dy == 0 ? 0 : dx == 0 ? 1 : dx == dy ? 2 : dx == -dy ? 3 : -1;
                if (axis >= 0)
                    profile.axes[p][axis] = std::min(8, profile.axes[p][axis] + weight);
            }
        }
        previous[p] = action.move;
    }
    return profile;
}
} // namespace gomoku
