//
// Created by koddnty on 2026/9/23.
//
#include "lock.hpp"
namespace m_sylar {
    auto g_logger = M_SYLAR_LOG_NAME("system");

Task<void> CoMutex::lock() {
    bool acquired = false;
    {
        // 只能在作用域内持锁: 若把unique_lock留在协程帧中, 它会一直持有到挂起之后,
        // 导致挂起路径上的tryAcquire()在同一线程上二次加锁而自死锁
        std::unique_lock<std::mutex> lk(m_mutex);
        if (!m_locked) {
            m_locked = true;
            acquired = true;
        }
    }
    if (!acquired) {
        co_await LockAwaiter(this);     // 返回时锁已由unlock()移交或由本协程抢到
    }
    co_return;
}

Task<void> CoMutex::unlock() {
    std::function<void()> waiter;
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        if (!m_locked) {
            M_SYLAR_ASSERT2(false, "unlock a unlocked co_mutex");
        }
        if (!m_waiters.empty()) {
            waiter = m_waiters.front();
            m_waiters.pop_front();
            // m_locked 保持 true，锁直接移交给 waiter
        } else {
            m_locked = false;
        }
    } // 出锁再调度
    if (waiter) {
        IOManager::getInstance()->schedule(waiter);
    }
    co_return;
}


// 当且仅当下面两个函数都返回true时，才可认为注册的认为会被唤醒
// 尝试获取锁，获取后返回true
bool CoMutex::tryAcquire() {
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_locked) {
        return false;
    }
    else {
        m_locked = true;
        return true;
    }
}

bool CoMutex::appendWaiter(std::function<void()> waiter) {       // 注意确保此函数执行时完成前，必须有人正在占用锁
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_locked) {        // 无人持有锁，返回
        return false;
    }
    m_waiters.push_back(waiter);
    return true;
}
}