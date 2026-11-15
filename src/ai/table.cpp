#include "ai/search.hpp"
namespace gomoku {
// Each entry is two words: the data (score, best move, depth, bound,
// generation) and the key XOR-ed with the data. Threads read and write without
// locks; an entry torn by a concurrent write fails the key check.
bool AI::probe(uint64_t key, Entry &entry) const {
    if (!table)
        return false;
    size_t slot = (key & (entries - 1)) * 2;
    uint64_t check = table[slot].load(std::memory_order_relaxed),
             data = table[slot + 1].load(std::memory_order_relaxed);
    if ((check ^ data) != key)
        return false;
    entry.score = static_cast<int32_t>(data & 0xFFFFFFFFu);
    entry.move = static_cast<int16_t>((data >> 32) & 0xFFFFu);
    entry.depth = static_cast<int8_t>((data >> 48) & 0xFFu);
    entry.flag = static_cast<int>((data >> 56) & 3u);
    entry.generation = static_cast<int>(data >> 58);
    return true;
}
void AI::store(uint64_t key, int score, int move, int depth, int flag) {
    if (!table)
        return;
    size_t slot = (key & (entries - 1)) * 2;
    uint64_t check = table[slot].load(std::memory_order_relaxed),
             old = table[slot + 1].load(std::memory_order_relaxed);
    // Keep a deeper entry of the current search for a different position.
    if ((check ^ old) != key && static_cast<int>(old >> 58) == generation &&
        static_cast<int8_t>((old >> 48) & 0xFFu) > depth)
        return;
    uint64_t data = uint64_t(uint32_t(score)) | uint64_t(uint16_t(move)) << 32 |
                    uint64_t(uint8_t(std::min(depth, 127))) << 48 |
                    uint64_t(flag | generation << 2) << 56;
    table[slot].store(key ^ data, std::memory_order_relaxed);
    table[slot + 1].store(data, std::memory_order_relaxed);
}
} // namespace gomoku
