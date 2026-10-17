#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace rust_wake {
// Rust RawWaker ownership: clone retains, wake consumes, wakeByRef borrows.
struct Wake {
    std::atomic<unsigned> references{1};
    std::mutex lock;
    std::condition_variable changed;
    bool signaled = false;
};
struct RawWaker { const void *table; Wake *data; };
inline void release(Wake *wake) {
    if (wake->references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete wake;
}
inline void signal(Wake *wake) {
    std::lock_guard<std::mutex> guard(wake->lock);
    wake->signaled = true;
    wake->changed.notify_one();
}
inline RawWaker cloneWake(Wake *);
inline void wakeOwned(Wake *wake) { signal(wake); release(wake); }
inline void wakeBorrowed(Wake *wake) { signal(wake); }
struct WakeTable {
    RawWaker (*clone)(Wake *);
    void (*wake)(Wake *);
    void (*wakeByRef)(Wake *);
    void (*drop)(Wake *);
};
inline constexpr WakeTable wakeTable{cloneWake, wakeOwned, wakeBorrowed, release};
inline RawWaker cloneWake(Wake *wake) {
    wake->references.fetch_add(1, std::memory_order_relaxed);
    return {&wakeTable, wake};
}
}

