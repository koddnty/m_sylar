#include <basic/tool.hpp>
#include <basic/log.h>


using namespace m_sylar;

static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");


void testPackedIDAllocatorBase() {
    // TODO 测试线程安全与性能
    PackedIDAllocator allocator(1024);
    std::vector<int> ids;
    for(int i = 0; i < 65535; ++i) {
        int id = allocator.alloc();
        if(id < 0) {
            M_SYLAR_LOG_ERROR(g_logger) << "alloc id failed, error code:" << (int)allocator.getErrorCode();
            break;
        }
    }

    for(int i = 0; i < 65535; ++i) {
        if(allocator.free(65534 - i) < 0) {
            M_SYLAR_LOG_ERROR(g_logger) << "free id failed, id:" << i << " error code:" << (int)allocator.getErrorCode();
        }
    }

    M_SYLAR_LOG_INFO(g_logger) << "test finished, First alocator plc:" << allocator.alloc();
}

void testPackedIDAllocatorExpand() {
    // TODO 测试线程安全与性能
    PackedIDAllocator allocator(2);
    std::vector<int> ids;
    for(int i = 0; i < 65535; ++i) {
        int id = allocator.alloc();
        if(id < 0) {
            M_SYLAR_LOG_ERROR(g_logger) << "alloc id failed, error code:" << (int)allocator.getErrorCode();
            break;
        }
    }

    for(int i = 0; i < 65535; ++i) {
        if(allocator.free(65534 - i) < 0) {
            M_SYLAR_LOG_ERROR(g_logger) << "free id failed, id:" << i << " error code:" << (int)allocator.getErrorCode();
        }
    }

    M_SYLAR_LOG_INFO(g_logger) << "Expand test finished, First alocator plc:" << allocator.alloc();
}


int main(void) {
    // testPackedIDAllocatorBase();
    testPackedIDAllocatorExpand();
    return 0;
}