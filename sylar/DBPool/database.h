#pragma once
#include <list>
#include <memory>
#include <atomic>
#include <functional>
#include "coroutine/corobase.h"

namespace m_sylar {


/**
    * 数据库连接池接口类, 连接池管理器需要继承此类并实现相关接口
    负责连接池的创建与连接销毁，但不负责扩容等操作，须实现对应接口
 */
template<typename ConnType, typename RespType>       // 数据库返回类型
class DBPool {
public: 
    using ptr = std::shared_ptr<DBPool>;
    
    enum State{
        INIT = 0,
        READY = 1,
        FULL = 2,
        CLOSED = 4,
        CLOSING = 8,
        ERROR = 16
    };

    DBPool(int min_conn, int max_conn) {
        m_state = INIT;
        if(min_conn < 0 || max_conn < 0 || max_conn < min_conn) {
            throw std::runtime_error("failed to create a msyql db connector manager, min_conn or max_conn invalid");
        }
        m_connectors.resize(max_conn);
        for(int i = 0; i < max_conn; i++) {
            m_connectors[i] = std::make_shared<ConnType>();
        }
        m_minConnector = min_conn;
        m_maxConnector = max_conn;
    }
    ~DBPool() {
    }

    bool checkRunState() {
        switch(m_state) {
            case INIT:
            case CLOSED:
            case CLOSING:
            case ERROR:
                return false;
            case READY:
            case FULL:
                return true;
        }
        return false;
    }
    void close() {
        m_state = CLOSING;
        tickle();       // 唤醒所有回调
    }

    virtual Task<std::shared_ptr<RespType>> executeQuery(const std::string& query) = 0;
    virtual int registeConnCb(std::function<void()> cb) = 0;        // 用于awaiter的回调
    virtual int tickle() = 0;                                       // 有新连接时的回调, 用于连接耗尽时阻塞控制, 若状态为关闭则全部tickle.

protected:
    virtual int borrowOneConn() = 0;                            // 线程不安全, 返回空闲连接索引
    virtual int returnConnn(int free_idx, bool isTimeOut = false) = 0;              // 线程不安全
    virtual int expand() = 0;                               // 线程安全, 扩展连接池, 返回扩容后连接数目, -1表示失败

protected: 
    std::list<int> m_freeConnInfos;                     // 空闲信息表[连接对应idx]
     
    std::vector<std::shared_ptr<ConnType>> m_connectors;           // 所有连接
    std::list<std::function<void()>> m_waitConnCb;                  // 有新连接可用时会唤醒其中一个任务，优先队列
    std::atomic<int> m_connectorCount = 0;                          // 当前连接数目, 记录所有已连接的连接, 若存在失败连接，会影响此值
    std::atomic<int> m_busyConnCount = 0;
    int m_minConnector;
    int m_maxConnector;
    std::atomic<State> m_state;
};

}