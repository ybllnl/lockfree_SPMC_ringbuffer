#include "gtest/gtest.h"
#include "SPMCRingBuffer.h"
#include <thread>
#include <set>
#include <vector>

class SPMCRingBufferTest : public ::testing::Test {
public:
    yblsys::SPMCRingBuffer<int> buffer;
    void SetUp() override {
        #ifdef DEBUG
        buffer.debug_producer_.store(0, std::memory_order_relaxed);
        #endif
    }

    uint64_t getHead() {
        return buffer.head.load(std::memory_order_relaxed);
    }

    uint64_t getTail() {
        return buffer.tail.load(std::memory_order_relaxed);
    }

    int getItem(uint64_t index) {
        return buffer.buffer[index & (YBLSYS_BUFFER_SIZE - 1)];
    }

    void producerTask(int num_items, std::vector<int>& items, std::set<int>& produced_set) {
        for(int i = 0; i < num_items; i++) {
            if(buffer.enqueue(items[i])){
                produced_set.insert(items[i]);
                std::cout << "Produced: " << items[i] << std::endl;
            }
        }
    }
    
    void consumerTask(int num_items, std::set<int>& consumed_set){
        for(int i = 0; i < num_items; i++){
            int item;
            while(!buffer.dequeue(item)){
                std::this_thread::yield();
            }
            std::cout << "Consumed: " << item << std::endl;
            EXPECT_EQ(consumed_set.find(item), consumed_set.end());
            consumed_set.insert(item);
        }
    }


};

TEST_F(SPMCRingBufferTest, Basic) {
    // assert head == tail == 0
    EXPECT_EQ(getHead(), 0);
    EXPECT_EQ(getTail(), 0);
    // enqueue 1
    buffer.enqueue(1);
    // assert head == 1
    EXPECT_EQ(getHead(), 1);
    EXPECT_EQ(getTail(), 0);
    EXPECT_EQ(getItem(0), 1);
    // enqueue 2
    buffer.enqueue(2);
    // assert head == 2
    EXPECT_EQ(getHead(), 2);
    EXPECT_EQ(getTail(), 0);
    EXPECT_EQ(getItem(0), 1);
    EXPECT_EQ(getItem(1), 2);
    // enqueue 3
    buffer.enqueue(3);
    // assert head == 3
    EXPECT_EQ(getHead(), 3);
    EXPECT_EQ(getTail(), 0);
    EXPECT_EQ(getItem(0), 1);
    EXPECT_EQ(getItem(1), 2);
    EXPECT_EQ(getItem(2), 3);
}

TEST_F(SPMCRingBufferTest, ProducerBufferFull) {
    for(int i = 0; i < YBLSYS_BUFFER_SIZE-1; i++) {
        buffer.enqueue(i);
    }
    // assert head == YBLSYS_BUFFER_SIZE-1
    EXPECT_EQ(getHead(), YBLSYS_BUFFER_SIZE-1);
    // enqueue one more
    buffer.enqueue(YBLSYS_BUFFER_SIZE-1);
    // drop the item
    EXPECT_EQ(getHead(), YBLSYS_BUFFER_SIZE-1);
    buffer.enqueue(0);
    // still drop the item
    EXPECT_EQ(getHead(), YBLSYS_BUFFER_SIZE-1);
}

TEST_F(SPMCRingBufferTest, ProducerConsumer) {
    std::vector<int> items;
    for(int i = 0; i < 10000; i++) {
        items.push_back(i);
    }
    std::set<int> produced_set;
    std::set<int> consumed_set1;
    std::set<int> consumed_set2;
    std::thread producer_thread(&SPMCRingBufferTest::producerTask, this, 10000, std::ref(items), std::ref(produced_set));
    std::thread consumer_thread1(&SPMCRingBufferTest::consumerTask, this, 5000, std::ref(consumed_set1));
    std::thread consumer_thread2(&SPMCRingBufferTest::consumerTask, this, 5000, std::ref(consumed_set2));
    producer_thread.join();
    consumer_thread1.join();
    consumer_thread2.join();
    EXPECT_EQ(produced_set.size(), consumed_set1.size() + consumed_set2.size());
    std::set<int> consumed_set;
    consumed_set.insert(consumed_set1.begin(), consumed_set1.end());
    consumed_set.insert(consumed_set2.begin(), consumed_set2.end());
    EXPECT_EQ(produced_set, consumed_set);
}