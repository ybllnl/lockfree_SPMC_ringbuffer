# lockfree_SPMC_ringbuffer
a lock-free, cache-optimized SPMC ring buffer designed with illustrative purpose for ultra-low latency systems

## Key Design Features

*   **Lock-Free:** Uses C++ `std::atomic` operations and carefully chosen memory orders (`acquire`/`release`/`relaxed`) to ensure thread safety between one producer and multiple consumers without mutexes.
*   **Cache Line Awareness:** Critical shared indices (`head` and `tail`) are padded and aligned to 64 bytes (`alignas(64)`) to prevent destructive interference ("false sharing") when accessed concurrently by producer and consumers running on different CPU cores.
*   **Power-of-2 Buffer Size:** Enforces a buffer size that is a power of 2, allowing expensive modulo (`%`) operations for index wrap-around to be replaced with cheap bitwise AND (`&`) operations.
*   **SPMC Focus:** Designed specifically for the single-producer scenario, allowing for simpler and faster enqueue logic compared to general MPMC queues.


## How to Build

This is a header-only library using C++17. To build the library:
```
cmake -B build
cd build/ && make
```

## API Logic Explained

The core logic relies on atomic head/tail counters and precise memory ordering.

### `enqueue(const T& item)`

*   **Context:** Called only by the **single producer** thread.
*   **Synchronization Goal:** Ensure that the write `buffer[index] = item` becomes visible to consumers *before* the `head` index update is visible. This maintains the invariant that data within the `[tail, head)` range (considering wrap-around) is always valid and ready to read.
*   **Implementation:**
    1.  Loads `head` (relaxed) and `tail` (`acquire`). The `acquire` load on `tail` is necessary to correctly check the buffer-full condition against the latest state visible from consumers.
    2.  Checks if full: `if (current_head - current_tail >= CAPACITY - 1)`. Uses the "one slot empty" convention. Returns `false` if full.
    3.  Writes the `item` to `buffer[current_head & (CAPACITY - 1)]`.
    4.  Stores the incremented `head` (`current_head + 1`) using `std::memory_order_release`. This `release` synchronizes with the consumers' `acquire` load of `head`, making the buffer write visible.

### `dequeue(T& item)`

*   **Context:** Can be called concurrently by **multiple consumer** threads.
*   **Synchronization Goals:**
    1.  Ensure a consumer only reads data that the producer has finished writing (by observing `head` with `acquire`).
    2.  Ensure only *one* consumer successfully claims and processes a specific item/slot.
    3.  Ensure the producer correctly sees when a slot has been consumed and is free (by observing `tail` with `acquire`).
*   **Implementation:**
    1.  Enters a loop, loading `tail` (relaxed initially).
    2.  Loads `head` using `std::memory_order_acquire`. This synchronizes with the producer's `release` store of `head`, ensuring any buffer writes associated with the observed `head` are visible.
    3.  Checks if empty: `if (current_head == current_tail)`. Returns `false` if empty.
    4.  Reads item speculatively: `T potential_item = buffer[current_tail & (CAPACITY - 1)];`
    5.  Attempts to atomically claim the slot using `tail.compare_exchange_weak(current_tail, next_tail, std::memory_order_acq_rel, std::memory_order_relaxed)`.
        *   **CAS:** Ensures only one consumer succeeds for a given `current_tail`.
        *   **`acq_rel` on Success:**
            *   The `acquire` synchronizes with the producer's `release` on `head` (ensuring the read item was valid).
            *   The `release` synchronizes with the producer's `acquire` on `tail` (signaling the slot is now free) and with other consumers (maintaining `tail`'s modification order).
        *   **`relaxed` on Failure:** Sufficient because the loop retries, and `current_tail` is updated by the CAS itself.
    6.  On successful CAS, moves `potential_item` to the output `item` and returns `true`.

## Benchmarking

A simple benchmark was conducted comparing this SPMC ring buffer against a standard `std::queue` protected by `std::mutex` and `std::condition_variable` (using efficient waiting, not busy-waiting).

*   **Setup:** 1 Producer thread, 2 Consumer threads.
*   **Data:** `struct Payload { uint32_t x4; }`
*   **Environment:** Ubuntu running inside a Virtual Machine (performance numbers are indicative of relative speedup, not absolute hardware capabilities).


```
Benchmarking: SPMC Ring Buffer (1P, 2C)...
Consumer <ID> consumed 5000000 items
Consumer <ID> consumed 5000000 items
Duration: 362 ms
Throughput: 27,624,309 items/sec
Benchmarking: std::queue + mutex (1P, 2C)...
Consumer (std) <ID> finished, consumed 5000000 items.
Consumer (std) <ID> finished, consumed 5000000 items.
Duration: 1399 ms
Throughput: 7,147,962 items/sec
```

Observation: In this specific 1P/2C test on a VM, the lock-free SPMC ring buffer demonstrated significantly higher throughput (~3.8x faster) compared to the lock-based std::queue. The actual speedup may vary greatly depending on the hardware, workload, and contention levels. The VM environment likely adds overhead, but the relative difference highlights the potential benefits of avoiding locks in high-contention scenarios.