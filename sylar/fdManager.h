#pragma once 
#include <cstdint>
#include <iostream>
#include <memory.h>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include "log.h"

/*
    为hook管理打开的socketfd， 确保ioManager可用性
    用户设置阻塞时，仅改变FdCtx不改变fd状态
    用户设置非阻塞，直接使用非阻塞的原函数
 */

namespace m_sylar
{

class FdCtx : std::enable_shared_from_this<FdCtx>
{
public:
    using ptr = std::shared_ptr<FdCtx>;

    FdCtx(int fd);
    ~FdCtx();

    bool init();

    bool is_init() const {return m_is_init;}
    bool is_socket() const {return m_is_socket;}
    bool is_closed() const {return m_is_closed;}
    bool close();

    void setUserNoblock(bool v) {m_userNoblock = v;}
    bool getUserNoblock() const {return m_userNoblock;}

    void setSysNoblock(bool v) {m_sysNoblock = v;}
    bool getSysNoblock() const {return m_sysNoblock;}

    void* setTimeout();
    uint64_t getTimeout();

private:
    bool m_is_init;
    bool m_is_closed;
    bool m_sysNoblock;
    bool m_userNoblock;
    bool m_is_socket;
    int m_fd;

    uint64_t m_recvTimeout;
    uint64_t m_sendTimeout;
};





class FdManager
{
public:
    using ptr = std::shared_ptr<FdManager>;

    FdManager();
    ~FdManager();

    FdCtx::ptr get(int fd, bool auto_create = false);
    void del(int fd);

private:
    std::shared_mutex m_rwMutex;
    std::vector<FdCtx::ptr> m_fdCtxs;
};

}
