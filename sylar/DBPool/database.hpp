#pragma once
#include <list>
#include <memory>
#include <atomic>
#include <functional>
#include "coroutine/corobase.h"

namespace m_sylar {

static Logger::ptr gdb_logger = M_SYLAR_LOG_NAME("system");




// 连接信息基础类定义 ------------------------------------------------------------
class ConnectInfoBase {
public:
    ConnectInfoBase() = default;
    virtual ~ConnectInfoBase() = default;

    std::string host;
    unsigned int port;
};









// 连接基本类定义 ------------------------------------------------------------
// handler类型
template<typename T>
concept DBConnectType = requires(T conn, ConnectInfoBase& info) {
    { conn.connect(info) } -> std::same_as<int>;          // 连接函数, 返回0表示成功, -1表示失败
};






// 类定义 ------------------------------------------------------------
template<DBConnectType ConnType, typename RespType>
class DBPool;





template<DBConnectType ConnType, typename RespType>
class ConnectWrapper : public std::enable_shared_from_this<ConnectWrapper<ConnType, RespType>> {
public:
    using ptr = std::shared_ptr<ConnectWrapper>;
    explicit ConnectWrapper(int conn_idx, std::shared_ptr<DBPool<ConnType, RespType>> pool);
    explicit ConnectWrapper(int conn_idx, DBPool<ConnType, RespType>* pool);
    explicit ConnectWrapper(const ConnectWrapper& other) = default;
    explicit ConnectWrapper(ConnectWrapper&& other) noexcept;
    ConnectWrapper& operator=(const ConnectWrapper& other) = delete;
    ConnectWrapper& operator=(ConnectWrapper&& other) noexcept;
    virtual ~ConnectWrapper();

    [[nodiscard]] virtual ConnType::ptr getConnector() { return m_connector; }
    [[nodiscard]] virtual int getConnIdx() { return m_conn_idx; }

    // 获取连接是否超时
    [[nodiscard]] bool getTimeout() const { return m_timeout; }
    void setTimeout(const bool timeout) { m_timeout = timeout; }

private:
    std::shared_ptr<ConnType> m_connector{nullptr};
    DBPool<ConnType, RespType>* m_pool{nullptr};
    int m_conn_idx {-1};
    bool m_timeout {false};
};









/**
    * 数据库连接池接口类, 连接池管理器需要继承此类并实现相关接口
    负责连接池的创建与连接销毁，但不负责扩容等操作，须实现对应接口
 */
template<DBConnectType ConnType, typename RespType>       // 数据库返回类型
class DBPool {
public:
    friend ConnectWrapper<ConnType, RespType>;
    using ptr = std::shared_ptr<DBPool>;
    enum State{
        INIT = 0,
        READY = 1,
        FULL = 2,
        CLOSED = 4,
        CLOSING = 8,
        ERROR = 16
    };


    DBPool(int min_conn, int max_conn);
    virtual ~DBPool() = default;

    [[nodiscard]] bool checkRunState() const ;
    void close() {
        m_state = CLOSING;
        tickle();       // 唤醒所有回调
    }

    virtual Task<std::shared_ptr<RespType>> executeQuery(const std::string& query) = 0;
    virtual int registeConnCb(std::function<void()> cb) = 0;        // 用于awaiter的回调
    virtual int tickle() = 0;                                       // 有新连接时的回调, 用于连接耗尽时阻塞控制, 若状态为关闭则全部tickle.

protected:
    virtual ConnectWrapper<ConnType, RespType>::ptr borrowOneConn();                                        // 线程不安全, 返回空闲连接索引
    virtual int returnConn(int conn_idx, bool isTimeOut);               // 线程不安全
    virtual int expand();                                               // 线程安全, 扩展连接池, 返回扩容后连接数目, -1表示失败
    void setIncreaseNum(size_t num) { m_increaseNum = num; }

protected:
    std::list<int> m_freeConnInfos;                                         // 空闲信息表[连接对应idx]
    std::shared_mutex m_mutex;                          // 连接池锁
    std::vector<std::shared_ptr<ConnType>> m_connectors;            // 所有连接
    std::list<std::function<void()>> m_waitConnCb;                  // 有新连接可用时会唤醒其中一个任务，优先队列
    std::atomic<int> m_connectorCount = 0;                          // 当前连接数目, 记录所有已连接的连接, 若存在失败连接，会影响此值
    std::atomic<int> m_busyConnCount = 0;
    int m_minConnector;
    int m_maxConnector;
    std::atomic<State> m_state;
    ConnectInfoBase m_connectorBaseInfo;
    size_t m_increaseNum {1};
};

}


#include "database.tpp"