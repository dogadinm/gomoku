#pragma once
#include <array>
#include <cassert>
#include <cstddef>
namespace gomoku {
// A board has at most 361 moves. Bounded search lists never allocate memory.
template <class T, std::size_t Capacity> class FixedList {
    std::array<T, Capacity> values; // only the first `count` are meaningful
    std::size_t count = 0;

  public:
    using iterator = T *;
    using const_iterator = const T *;
    std::size_t size() const {
        return count;
    }
    bool empty() const {
        return count == 0;
    }
    void clear() {
        count = 0;
    }
    void resize(std::size_t n) {
        assert(n <= count);
        count = n;
    }
    void push_back(const T &value) {
        assert(count < Capacity);
        values[count++] = value;
    }
    T &operator[](std::size_t i) {
        return values[i];
    }
    const T &operator[](std::size_t i) const {
        return values[i];
    }
    T &front() {
        return values[0];
    }
    const T &front() const {
        return values[0];
    }
    iterator begin() {
        return values.data();
    }
    iterator end() {
        return values.data() + count;
    }
    const_iterator begin() const {
        return values.data();
    }
    const_iterator end() const {
        return values.data() + count;
    }
};
} // namespace gomoku
