#pragma once

#include "core/network/NetPacket.h"

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <deque>
#include <mutex>

namespace Engine {
namespace Platform {
namespace Networking {

template <typename T>
class MessageQueue {
public:
    void Push(T item) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_items.push_back(std::move(item));
        }
        m_cv.notify_one();
    }

    bool TryPop(T& out) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_items.empty()) {
            return false;
        }
        out = std::move(m_items.front());
        m_items.pop_front();
        return true;
    }

    bool WaitPop(T& out, int timeoutMs) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() { return !m_items.empty() || m_stopped; })) {
            return false;
        }
        if (m_stopped || m_items.empty()) {
            return false;
        }
        out = std::move(m_items.front());
        m_items.pop_front();
        return true;
    }

    void Stop() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopped = true;
        }
        m_cv.notify_all();
    }

    void Reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_items.clear();
        m_stopped = false;
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_items.size();
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<T> m_items;
    bool m_stopped = false;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
