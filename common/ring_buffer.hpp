#pragma once

#include <cstdint>
#include <atomic>

template <typename T>
class ring_buffer {
    T *data{0};
    size_t cap{0};
    atomic<size_t> head{0}, tail{0}, write{0};

public:
    ring_buffer() = default;
    ring_buffer(const size_t &_cap) {
        cap = _cap;
        data = new T[cap];
    }
    ring_buffer(const ring_buffer&) = delete;
    ~ring_buffer() {
        if (data) {
            delete[] data;
        }
    }
    ring_buffer &operator=(const ring_buffer&) = delete;
    ring_buffer &operator=(const ring_buffer&) volatile = delete;

    int push(const T &val) {
        size_t t, w;
        do {
            t = tail.load(memory_order_relaxed); // (1)
            if ((t + 1) % cap == head.load(memory_order_acquire)) //(2)
                return -1;
        } while (!tail.compare_exchange_weak(t, (t + 1) % cap, memory_order_relaxed)); // (3)
        data[t] = val; // (4), (4) happens-before (8)
        do {
            w = t;
        } while (!write.compare_exchange_weak(w, (w + 1) % cap,
                memory_order_release, memory_order_relaxed)); // (5), (5) synchronizes-with (7)
        return 0;
    }

    int pop(T &val) {
        size_t h;
        do {
            h = head.load(memory_order_relaxed); // (6)
            if (h == write.load(memory_order_acquire)) // (7)
                return -1;
            val = data[h]; // (8), (8) happens-before (4)
        } while (!head.compare_exchange_strong(h, (h + 1) % cap,
                memory_order_release, memory_order_relaxed)); // (9), (9) synchronizes-with (2)
        return 0;
    }
};