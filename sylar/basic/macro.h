#pragma once

#include <string>
#include <assert.h>

#if defined __GUNC__ || defined __llvm__
#define M_SYLAR_LIKELY(x) __builtin_expect(!!(x), 1)
#define M_SYLAR_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define M_SYLAR_LIKELY(x) (x)
#define M_SYLAR_UNLIKELY(x) (x)
#endif


#define M_SYLAR_ASSERT(x) \
    if(M_SYLAR_UNLIKELY(!(x))) {  \
        M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "ASSERTION: " #x \
            << "\nBackTraces:\n" << m_sylar::BackTraceToStr(100, 2, "    "); \
        assert(x); \
    }

#define M_SYLAR_ASSERT2(x, w) \
    if(M_SYLAR_UNLIKELY(!(x))) {  \
        M_SYLAR_LOG_ERROR(M_SYLAR_GET_LOGGER_ROOT()) << "ASSERTION: " #x \
            << "\n" #w \
            << "\nBackTraces:\n" << m_sylar::BackTraceToStr(100, 2, "    "); \
        assert(x); \
    }
