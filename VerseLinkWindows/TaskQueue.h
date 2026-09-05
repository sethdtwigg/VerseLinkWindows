#pragma once
#ifndef TaskQueue_H
#define TaskQueue_H

#include <condition_variable>
#include <mutex>

// Single-slot hand-off between the UI thread and the verse worker thread.
//
// "Single-slot" is deliberate: while a task is pending, further posts are
// coalesced, so holding the hotkey down does not queue a burst of clipboard
// operations. The flag is mutated under the same mutex the waiter blocks on -
// setting it outside that mutex lets a notify land between the waiter's
// predicate check and its registration on the condition variable, and the
// wakeup is lost. That left the app permanently deaf to the hotkey, because
// the stuck pending flag then coalesced every later press away.
//
// Extracted from VerseLinkWindows.cpp so the race is testable without a
// message pump or a GUI.
class TaskQueue {
public:
    // Requests a task run. Returns false when one was already pending and this
    // request was coalesced into it.
    bool Post();

    // Blocks until a task is pending or Shutdown() is called. Returns true when
    // a task should run (and consumes it), false when the caller should exit.
    bool WaitForTask();

    // Wakes every waiter and makes all further WaitForTask() calls return false.
    void Shutdown();

    bool IsShuttingDown() const;

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_taskPending = false;
    bool m_shuttingDown = false;
};

#endif
