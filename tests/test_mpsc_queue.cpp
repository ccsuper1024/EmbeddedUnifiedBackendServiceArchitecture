#include "mpsc_queue.h"

#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

TEST(MpscQueueTest, SingleThreadPushPopKeepsOrder) {
  backend::MpscQueue<int> queue(8);
  for (int i = 0; i < 8; ++i) {
    EXPECT_TRUE(queue.Push(i));
  }
  int value = 0;
  for (int i = 0; i < 8; ++i) {
    EXPECT_TRUE(queue.Pop(value));
    EXPECT_EQ(value, i);
  }
  EXPECT_FALSE(queue.Pop(value));
}

TEST(MpscQueueTest, MultiProducerSingleConsumerTransfersAllItems) {
  backend::MpscQueue<int> queue(1024);
  const int producer_count = 4;
  const int items_per_producer = 256;
  std::atomic<int> produced{0};
  std::atomic<int> consumed{0};
  std::vector<std::thread> producers;
  for (int p = 0; p < producer_count; ++p) {
    producers.emplace_back([&queue, &produced, items_per_producer]() {
      for (int i = 0; i < items_per_producer; ++i) {
        int value = i;
        while (!queue.Push(value)) {
          std::this_thread::yield();
        }
        ++produced;
      }
    });
  }
  int total = producer_count * items_per_producer;
  int value = 0;
  while (consumed.load() < total) {
    if (queue.Pop(value)) {
      ++consumed;
    } else {
      std::this_thread::yield();
    }
  }
  for (auto& t : producers) {
    t.join();
  }
  EXPECT_EQ(produced.load(), total);
  EXPECT_EQ(consumed.load(), total);
}

