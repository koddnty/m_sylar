#include "timer_task.hpp"


namespace m_sylar {


Task<bool> TimeTask::runner() {
    bool rt = true;
    if(m_is_canceled == false) {
        rt = true;
    }
    if(m_condition) {
        if(co_await m_condition()) {
            co_await m_main_cb();
        }
        else if(m_condition_cb) {
            co_await m_condition_cb();
        }
        rt = true;
    }
    else {
        co_await m_main_cb();
        rt = true;
    }
    next();
    co_return rt;
}

uint64_t TimeTask::next() {
    if(m_is_cycle) {
         m_execute_ms += m_intreval_ms;
    }
    else {
        m_execute_ms = 0;
    }
    return m_execute_ms;
}




}
