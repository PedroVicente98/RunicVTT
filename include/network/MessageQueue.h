#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class MessageQueue {
private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;

public:
    void push(T value) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        lock.unlock();
        condition_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !queue_.empty(); });
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    bool try_pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }
};
