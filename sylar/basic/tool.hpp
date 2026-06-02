#include <memory>
#include <vector>
#include <mutex>
namespace m_sylar {

class PackedIDAllocator{
public:
    using ptr = std::shared_ptr<PackedIDAllocator>;
    // 最小单位为64，1024即可分配65536个ID
    PackedIDAllocator(int size);
    ~PackedIDAllocator() {}
    enum class ErrorCode{
        SUCCESS = 0,              // 默认错误
        INVALID_ID = -1,        // 无效ID
        ALREADY_FREED = -2,         // ID已经被释放过了
        NO_AVAILABLE_ID = -3,       // 没有可用ID了
        EXPAND_FAILED = -4          // 扩展ID失败
    };

    /**
        @brief 分配一个ID，成功返回ID，失败返回-1
            线程安全，支持多线程并发分配和释放ID，但不保证分配顺序
    */
    int alloc();
            
    /**
        @brief 释放一个ID，成功返回0，失败返回-1

        @return 参数错误码
    */
    int free(int id);

    uint64_t getSize() const { return m_bit_map.size() * 64; }

    ErrorCode getErrorCode() const { return error_code; }


private:
    int expand();       // 线程不安全，需要外部加锁

    std::mutex m_global_mutex;    // 全局锁，用于保护m_bit_map的扩展和更新
    std::vector<uint64_t> m_bit_map;
    uint64_t m_next_chunk{0};
    int m_next_bit{0};

    ErrorCode error_code {ErrorCode::SUCCESS};
};
}
