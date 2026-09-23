#pragma once
#include "coroutine/corobase.h"


namespace m_sylar {

// 锁
class LockAwaiter;

class CoMutex {
public:
    Task<void> lock();
    Task<void> unlock();


    // 当且仅当下面两个函数都返回true时，才可认为注册的认为会被唤醒
    // 尝试获取锁，获取后返回true
    bool tryAcquire();

    bool appendWaiter(std::function<void()> waiter);

private:
    std::mutex m_mutex;
    bool m_locked {false};
    std::list<std::function<void()>> m_waiters;
};



class LockAwaiter : public m_sylar::Awaiter<void> {
public:
    explicit LockAwaiter(CoMutex* mutex) : m_mutex(mutex) {}
    ~LockAwaiter() override = default;
    bool await_ready() override {
        return m_mutex->tryAcquire();
    }

protected:
    void on_suspend() override {
        while (true) {       // 循环注册waiter直到成功
            bool rt = m_mutex->appendWaiter([this] {
                resume();
            });
            if (rt) {break; }
            rt = m_mutex->tryAcquire();
            if (rt) {       // 获得锁,恢复
                resume();
                break;
            }
        }
    }
    void before_resume() override {}


private:
    CoMutex* m_mutex {nullptr};
};



};
