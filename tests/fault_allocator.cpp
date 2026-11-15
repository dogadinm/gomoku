#include "fault.hpp"
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <new>
namespace {
std::atomic<long> remaining{-1}, failed{0};
static_assert(std::atomic<long>::is_always_lock_free, "signal control needs lock-free atomics");
bool reject() {
    long n = remaining.load(std::memory_order_relaxed);
    while (n >= 0) {
        if (n == 0) {
            failed.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        if (remaining.compare_exchange_weak(n, n - 1, std::memory_order_relaxed))
            return false;
    }
    return false;
}
void *allocate(std::size_t size) {
    if (reject())
        throw std::bad_alloc();
    if (void *p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void *aligned(std::size_t size, std::size_t alignment) {
    if (reject())
        throw std::bad_alloc();
    void *p = nullptr;
    if (posix_memalign(&p, alignment, size ? size : alignment) == 0)
        return p;
    throw std::bad_alloc();
}
void failSignal(int) {
    remaining.store(0, std::memory_order_relaxed);
}
void recoverSignal(int) {
    remaining.store(-1, std::memory_order_relaxed);
}
struct Install {
    Install() {
        std::signal(SIGUSR1, failSignal);
        std::signal(SIGUSR2, recoverSignal);
    }
} install;
} // namespace
namespace fault {
void arm(long count) {
    failed.store(0);
    remaining.store(count);
}
void disable() {
    remaining.store(-1);
}
long failures() {
    return failed.load();
}
} // namespace fault
void *operator new(std::size_t n) {
    return allocate(n);
}
void *operator new[](std::size_t n) {
    return allocate(n);
}
void *operator new(std::size_t n, const std::nothrow_t &) noexcept {
    try {
        return allocate(n);
    } catch (...) {
        return nullptr;
    }
}
void *operator new[](std::size_t n, const std::nothrow_t &) noexcept {
    try {
        return allocate(n);
    } catch (...) {
        return nullptr;
    }
}
void operator delete(void *p) noexcept {
    std::free(p);
}
void operator delete[](void *p) noexcept {
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept {
    std::free(p);
}
void operator delete[](void *p, std::size_t) noexcept {
    std::free(p);
}
void operator delete(void *p, const std::nothrow_t &) noexcept {
    std::free(p);
}
void operator delete[](void *p, const std::nothrow_t &) noexcept {
    std::free(p);
}
void *operator new(std::size_t n, std::align_val_t a) {
    return aligned(n, static_cast<std::size_t>(a));
}
void *operator new[](std::size_t n, std::align_val_t a) {
    return aligned(n, static_cast<std::size_t>(a));
}
void operator delete(void *p, std::align_val_t) noexcept {
    std::free(p);
}
void operator delete[](void *p, std::align_val_t) noexcept {
    std::free(p);
}
void operator delete(void *p, std::size_t, std::align_val_t) noexcept {
    std::free(p);
}
void operator delete[](void *p, std::size_t, std::align_val_t) noexcept {
    std::free(p);
}
