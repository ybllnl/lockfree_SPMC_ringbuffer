#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <numeric>
#include <atomic>
#include <cstdint> // For uint32_t

// --- Include your Ring Buffer and the standard queue ---
#include "SPMCRingBuffer.h" // Adjust path if needed (relative to build)
#include <queue>
#include <mutex>
#include <condition_variable> // Needed for more robust std::queue handling

// --- Data Structure ---
struct Payload { 
    uint32_t data1;
    uint32_t data2;
    uint32_t data3;
    uint32_t id;

    // Default constructor needed for buffer array initialization
    Payload() : data1(0), data2(0), data3(0), id(0) {}
    // This is arbitrary data, just for testing
    Payload(uint32_t id) : data1(id), data2(id+1), data3(id+2), id(id) {}

    // Needed for comparison later if using vector equality check
    bool operator<(const Payload& other) const {
        return id < other.id;
    }
     bool operator==(const Payload& other) const {
        return id == other.id;
    }
};

// --- Configuration ---
// Make sure SPMCRingBuffer template uses this size
const long long NUM_CONSUMERS = 2;
const long long NUM_TASKS_PER_CONSUMER = 5 * 1000 * 1000;
const long long NUM_ITEMS = NUM_CONSUMERS * NUM_TASKS_PER_CONSUMER;  

yblsys::SPMCRingBuffer<Payload> spmc_buffer;
std::queue<Payload> std_queue;
std::mutex queue_mutex;
std::condition_variable cv_consumer;

// --- Producer Task ---
void producer_spmc() {
    for (long long i = 0; i < NUM_ITEMS; ++i) {
        Payload p(static_cast<uint32_t>(i));
        while (!spmc_buffer.enqueue(p)) {
             std::this_thread::yield(); // Simple spin/yield if full
        }
    }
}

void producer_std() {
    for (long long i = 0; i < NUM_ITEMS; ++i) {
        Payload p(static_cast<uint32_t>(i));
        { // Scope for lock
            std::unique_lock<std::mutex> lock(queue_mutex);
            std_queue.push(p);
        } // Lock released
        cv_consumer.notify_one();
    }
}


// --- Consumer Task ---
void consumer_spmc() {
    Payload item;
    long long count = 0;
    // Keep consuming as long as producer might still be running OR buffer not empty
    for (long long i = 0; i < NUM_TASKS_PER_CONSUMER; ++i) {
        while (!spmc_buffer.dequeue(item)) {
            std::this_thread::yield();
        }
        count++;
    }
    std::cout << "Consumer " << std::this_thread::get_id() << " consumed " << count << " items" << std::endl;
}

void consumer_std() {
    Payload item;
    long long count = 0;
    for (long long i = 0; i < NUM_TASKS_PER_CONSUMER; ++i) {
        { // Scope for the lock guard
            std::unique_lock<std::mutex> lock(queue_mutex);

            cv_consumer.wait(lock, []{ return !std_queue.empty(); });

            item = std_queue.front();
            std_queue.pop();
            count++;
        }
    }

    // Update global counter atomically after finishing the loop
    std::cout << "Consumer (std) " << std::this_thread::get_id() << " finished, consumed " << count << " items." << std::endl;
}

// --- Timing Function ---
template <typename ProducerFunc, typename ConsumerFunc>
void time_queue(const std::string& name, ProducerFunc producer_func, ConsumerFunc consumer_func) {
    std::cout << "Benchmarking: " << name << " (1P, 2C)..." << std::endl;
    // Create threads
    std::thread consumer_thread1(consumer_func);
    std::thread consumer_thread2(consumer_func);
    std::thread producer_thread(producer_func);

    // Start timing and signal threads
    auto start_time = std::chrono::high_resolution_clock::now();

    // Wait for threads to complete
    producer_thread.join();
    consumer_thread1.join();
    consumer_thread2.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    double throughput = (double)NUM_ITEMS / (duration.count() / 1000.0); // Items per second

    std::cout << "  Duration: " << duration.count() << " ms" << std::endl;
    std::cout << "  Throughput: " << (long long)throughput << " items/sec" << std::endl;
    std::cout << "--------------------------" << std::endl;
}


int main() {
    std::cout << "Simple SPMC Benchmark (1 Producer, 2 Consumers)" << std::endl;
    std::cout << "Items to Process: " << NUM_ITEMS << std::endl;
    std::cout << "--------------------------" << std::endl;

    // Run SPMC Benchmark
    time_queue("SPMC Ring Buffer", producer_spmc, consumer_spmc);

    // Run std::queue Benchmark
    time_queue("std::queue + mutex", producer_std, consumer_std);

    return 0;
}
