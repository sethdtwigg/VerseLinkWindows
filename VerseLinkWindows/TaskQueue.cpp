#include "TaskQueue.h"

bool TaskQueue::Post() {
    bool queued = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_shuttingDown) {
            return false;
        }
        if (!m_taskPending) {
            m_taskPending = true;
            queued = true;
        }
    }
    // Notifying outside the lock avoids waking the worker straight onto a
    // mutex we still hold; the flag itself was set under it, which is what
    // makes the wakeup impossible to lose.
    if (queued) {
        m_cv.notify_one();
    }
    return queued;
}

bool TaskQueue::WaitForTask() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_shuttingDown || m_taskPending; });

    if (m_shuttingDown) {
        return false;
    }

    m_taskPending = false;
    return true;
}

void TaskQueue::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shuttingDown = true;
    }
    m_cv.notify_all();
}

bool TaskQueue::IsShuttingDown() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_shuttingDown;
}
