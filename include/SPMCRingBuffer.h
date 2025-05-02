
#ifndef YBLSYS_SPMCRINGBUFFER_H
#define YBLSYS_SPMCRINGBUFFER_H

#include <atomic>

#define YBLSYS_BUFFER_SIZE 1024
namespace yblsys {


    template<typename T>
    class SPMCRingBuffer {
        public:
        struct alignas(64) IndexPadded : std::atomic<uint64_t> {
            using std::atomic<uint64_t>::atomic;
        };


        /**
         * @brief Enqueue an item to the buffer
         * 
         * @param item 
         * @return true if the item is successfully enqueued, false if the buffer is full
         */
        bool enqueue(const T& item){
            // 1. check whether there is only one producer in debug mode
            #ifdef DEBUG
            if(debug_producer_.exchange(1, std::memory_order_relaxed)){
                std::cerr << "Only one producer is allowed" << std::endl;
                assert(false && "Multiple producers detected.");
            }
            #endif
            // 2. write the item to the buffer
            // this tail load can actually be relaxed, but it is very very tricky and even change slightly of the behavior, it can break, so we use acquire here
            auto current_tail = tail.load(std::memory_order_acquire);
            auto current_head = head.load(std::memory_order_relaxed);
            // note that the distance between the head and the tail is at most YBLSYS_BUFFER_SIZE - 1
            if(current_head - current_tail >= YBLSYS_BUFFER_SIZE - 1){
                // we are full, we drop the item for low latency and this should be rare
                // leave one slot to check the full condition and distinguish the full and empty
                return false;
            }
            buffer[current_head & (YBLSYS_BUFFER_SIZE - 1)] = item;
            // 3. increment the producer index
            head.store(current_head + 1, std::memory_order_release);
            // return
            return true;
        }

        /**
         * @brief Dequeue an item from the buffer
         * 
         * @param item 
         * @return true if the item is successfully dequeued, false if the buffer is not ready to be consumed
         */
        bool dequeue(T& item){
            // get the current tail
            auto current_tail = tail.load(std::memory_order_relaxed);
            while(true){
                // compare with the head to verify whether the current buffer item is ready to be consumed
                auto next_tail = current_tail + 1;
                auto current_head = head.load(std::memory_order_acquire);
                if(current_head == current_tail){
                    // current buffer item is not ready to be consumed
                    return false;
                }
                // make sure read the item before producer may write to the slot
                T potential_item = buffer[current_tail & (YBLSYS_BUFFER_SIZE - 1)];
                // guarantee that [head, tail) is the range of items that have been consumed, so we must have the potential item.
                if(tail.compare_exchange_weak(current_tail, next_tail, std::memory_order_acq_rel, std::memory_order_relaxed)){
                    item = std::move(potential_item);
                    return true;
                }
            }
        }


        SPMCRingBuffer() = default;

        /***
         * property: (always think as an verification problem)
         * 1. [tail, head), or [tail, BUFFER_SIZE) U [0, head) is the range of the valid, ready to be consumed items
         * 2. 0 <= head - tail <= BUFFER_SIZE - 1
         * 3. slots [head, tail) or wrap over [head, BUFFER_SIZE) U [0, tail) is the range of items that have been consumed.
         */        
        IndexPadded head = 0;
        IndexPadded tail = 0;
        T buffer[YBLSYS_BUFFER_SIZE];


        private:
        #ifdef DEBUG
        std::atomic<bool> debug_producer_ = false;
        #endif


    };

}



#endif // YBLSYS_SPMCRINGBUFFER_H
