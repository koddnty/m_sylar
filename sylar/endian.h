#pragma once

#include <cstdint>
#include <locale>
#include <byteswap.h>


/*
    实现在不同机器进行大小端转化
*/
namespace m_sylar
{
template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type
byteSwap(T value)
{   
    return (T)bswap_64(value);
}

template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type
byteSwap(T value)
{   
    return (T)bswap_32(value);
}

template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type
byteSwap(T value)
{   
    return (T)bswap_16(value);
}


#if BYTE_ORDER == BIG_ENDIAN
#define M_SYLAR_BYTE_ORDER BIG_ENDIAN
#else 
#define M_SYLAR_BYTE_ORDER LITTLE_ENDIAN
#endif

// 以本机视角，把数据转化为大端或小端
#if M_SYLAR_BYTE_ORDER == BIG_ENDIAN
template<typename T>
T byteSwapToLittleEndian(T t)
{
    return byteSwap(t);
}

template<typename T>
T byteSwapToBigEndian(T t)
{
    return t;
}

template<typename T>
T NetByteSwapToHostEndian(T t)
{
    return t;
}
#else
template<typename T>
T byteSwapToLittleEndian(T t)           // 本机字节序序到小端
{
    return t;
}

template<typename T>
T byteSwapToBigEndian(T t)              // 本机字节序序到大端
{
    return byteSwap(t);
}

template<typename T>
T NetByteSwapToHostEndian(T t)          // 网络字节序到本机字节序
{
    return byteSwap(t);
}
#endif
}