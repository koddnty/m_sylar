#pragma once

#include <string>
#include <assert.h>

#define M_SYLAR_ASSERT(x) \
    if(!(x)) {  \
        M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "ASSERTION: " #x \
            << "\nBackTraces:\n" << m_sylar::BackTraceToStr(100, 2, "    "); \
        assert(x); \
    }

#define M_SYLAR_ASSERT2(x, w) \
    if(!(x)) {  \
        M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "ASSERTION: " #x \
            << "\n" #w \
            << "\nBackTraces:\n" << m_sylar::BackTraceToStr(100, 2, "    "); \
        assert(x); \
    }
