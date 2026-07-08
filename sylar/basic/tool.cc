#include "tool.hpp"

namespace m_sylar {
PackedIDAllocator::PackedIDAllocator(int size)
{
    m_bit_map.resize(size, {});
}

int PackedIDAllocator::alloc() {
    // 获得可用ID并标记为已分配
    std::unique_lock<std::mutex> wlock(m_global_mutex);  
    int rt = sizeof(m_bit_map[m_next_chunk]) * m_next_chunk * 8 + m_next_bit;

    m_bit_map[m_next_chunk] |= (1ULL << m_next_bit);  // 将对应位标记为1，表示已分配

    // 更新下一个可用ID位置
    bool isFound = false;
    for(int i = m_next_chunk; i < m_bit_map.size(); ++i) {
        if(m_bit_map[i] != UINT64_MAX) {
            m_next_chunk = i;
            m_next_bit = std::countr_zero(~m_bit_map[i]);  // 找到第一个0位
            isFound = true;
            break;
        }   
    }

    if(!isFound) { // 没有可用ID了，尝试扩展
        // 预设
        m_next_chunk = m_bit_map.size();
        m_next_bit = 0;
        if((int)(expand()) < 0) {
            m_next_chunk = 0;
            m_next_bit = -1;
        }
    }

    return rt;
}

int PackedIDAllocator::free(int id) {
    std::unique_lock<std::mutex> wlock(m_global_mutex);
    if(id > getSize()) {
        error_code = ErrorCode::INVALID_ID;
        return -1;
    }

    int chunk_index = id / 64;
    int bit_index = id % 64;
    uint64_t mask = 1ULL << bit_index;
    if((m_bit_map[chunk_index] & mask) == 0) {
        error_code = ErrorCode::ALREADY_FREED;
        return -1;
    }
    m_bit_map[chunk_index] &= ~mask;  // 将对应位标记为0，表示已释放

    if(chunk_index < m_next_chunk || (chunk_index == m_next_chunk && bit_index < m_next_bit)) {
        m_next_chunk = chunk_index;
        m_next_bit = bit_index;
    }
    return 0;
}

int PackedIDAllocator::expand() {
    try{
        m_bit_map.resize(m_bit_map.size() * 2, {});
    }
    catch(const std::exception& e) {
        error_code = ErrorCode::EXPAND_FAILED;
        return -1;
    }
    return 0;
}

}