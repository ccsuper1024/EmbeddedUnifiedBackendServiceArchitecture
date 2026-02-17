#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace backend {

template <typename T>
class MpscQueue {
 public:
  explicit MpscQueue(std::size_t capacity)
      : buffer_(capacity),
        capacity_(capacity),
        head_(0),
        tail_(0) {
  }

  bool Push(const T& value) {
    return Enqueue(value);
  }

  bool Push(T&& value) {
    return Enqueue(std::move(value));
  }

  bool Pop(T& out) {
    std::size_t head = head_.load(std::memory_order_acquire);
    if (tail_ == head) {
      return false;
    }
    std::size_t index = tail_ % capacity_;
    out = std::move(buffer_[index]);
    ++tail_;
    return true;
  }

 private:
  template <typename U>
  bool Enqueue(U&& value) {
    while (lock_.test_and_set(std::memory_order_acquire)) {
    }
    std::size_t head = head_.load(std::memory_order_relaxed);
    std::size_t used = head - tail_;
    if (used >= capacity_) {
      lock_.clear(std::memory_order_release);
      return false;
    }
    std::size_t index = head % capacity_;
    buffer_[index] = std::forward<U>(value);
    head_.store(head + 1, std::memory_order_release);
    lock_.clear(std::memory_order_release);
    return true;
  }

  std::vector<T> buffer_;
  const std::size_t capacity_;
  std::atomic<std::size_t> head_;
  std::size_t tail_;
  std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
};

}  // namespace backend

