#pragma once


namespace m_sylar {

// 连接包装器函数定义 ------------------------------------------------------------
template<DBConnectType ConnType, typename RespType>
ConnectWrapper<ConnType, RespType>::ConnectWrapper(const int conn_idx, std::shared_ptr<ConnType> conn_ptr, std::shared_ptr<DBPool<ConnType, RespType>> pool)
    : m_connector(conn_ptr), m_pool(pool.get()), m_conn_idx(conn_idx) {
}

template<DBConnectType ConnType, typename RespType>
ConnectWrapper<ConnType, RespType>::ConnectWrapper(const int conn_idx, std::shared_ptr<ConnType> conn_ptr, DBPool<ConnType, RespType>* pool)
    : m_connector(conn_ptr), m_pool(pool), m_conn_idx(conn_idx) {
}

template<DBConnectType ConnType, typename RespType>
ConnectWrapper<ConnType, RespType>::ConnectWrapper(ConnectWrapper&& other) noexcept {
    // 移动资源
    m_conn_idx = other.m_conn_idx;
    m_connector = other.m_connector;
    m_pool = other.m_pool;
    // 清理资源
    other.m_conn_idx = -1;
    other.m_connector = nullptr;
    other.m_pool = nullptr;
}

template<DBConnectType ConnType, typename RespType>
ConnectWrapper<ConnType, RespType>& ConnectWrapper<ConnType, RespType>::operator=(ConnectWrapper&& other) noexcept{
    // 移动资源
    m_conn_idx = other.m_conn_idx;
    m_connector = other.m_connector;
    m_pool = other.m_pool;
    // 清理资源
    other.m_conn_idx = -1;
    other.m_connector = nullptr;
    other.m_pool = nullptr;
    return *this;
}

template<DBConnectType ConnType, typename RespType>
ConnectWrapper<ConnType, RespType>::~ConnectWrapper() {
    if(m_conn_idx < 0) {
        return;
    }

    // 归还连接
    m_pool->returnConn(m_conn_idx, getTimeout());


    // 清空
    m_conn_idx = -1;
    m_connector.reset();
    m_pool = nullptr;
}











// 连接池基本函数定义 ------------------------------------------------------------
template<DBConnectType ConnType, typename RespType>
DBPool<ConnType, RespType>::DBPool(const int min_conn,const  int max_conn) {
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




template<DBConnectType ConnType, typename RespType>
[[nodiscard]] bool DBPool<ConnType, RespType>::checkRunState() const {
    switch(m_state) {
        case INIT:
        case CLOSED:
        case CLOSING:
        case ERROR:
            return false;
        case READY:
        case FULL:
            return true;
        default:
            return false;
    }
}






// 连接可用性管理
template<DBConnectType ConnType, typename RespType>
int DBPool<ConnType, RespType>::registeConnCb(const std::function<void()>& cb) {
    if(!checkRunState()) {
        M_SYLAR_LOG_WARN(gdb_logger) << "failed to registe Connect callback, connect pool is not running";
        return -1;
    }
    if(!cb) {
        M_SYLAR_LOG_ERROR(gdb_logger) << "failed to registe Connect callback, parameter cn is null";
        return -1;
    }
    std::unique_lock w_lock(m_mutex);
    if(m_state != FULL) { // 直接运行
        IOManager::getInstance()->schedule(TaskCoro20::create_func(cb));
    }
    else {
        m_waitConnCb.push_back(cb);
    }
    return 0;
}



template<DBConnectType ConnType, typename RespType>
int DBPool<ConnType, RespType>::tickle() {
    if(m_waitConnCb.empty()) {return 0;}

    std::list<std::function<void()>> temp_tasks;

    std::unique_lock w_lock(m_mutex);
    if(m_state == State::CLOSING) {
        temp_tasks.splice(temp_tasks.begin(), m_waitConnCb, m_waitConnCb.begin(), m_waitConnCb.end());
    }
    else {
        const auto task_begin = m_waitConnCb.begin();
        const auto task_final = std::next(task_begin, std::min(m_freeConnInfos.size(), m_waitConnCb.size()) );
        temp_tasks.splice(temp_tasks.begin(), m_waitConnCb, task_begin, task_final);
    }
    w_lock.unlock();

    for(const auto& it : temp_tasks) {
        IOManager::getInstance()->schedule(TaskCoro20::create_func(it));            // 可行否？<...>
    }
    return 0;
}






// 连接池连接管理函数定义
template<DBConnectType ConnType, typename RespType>
ConnectWrapper<ConnType, RespType>::ptr DBPool<ConnType, RespType>::borrowOneConn() {
    std::unique_lock w_lock(m_mutex);
    if(!checkRunState() || m_state == State::FULL) {
        return std::make_shared<ConnectWrapper<ConnType, RespType>>(ConnectWrapper<ConnType, RespType>(-1, nullptr, nullptr));
    }

    if(m_state == READY) {
        // 获取资源序号
        if(m_freeConnInfos.empty()) {
            M_SYLAR_LOG_ERROR(gdb_logger) << "state is not consistent with freeConnInfos[]";
            return std::make_shared<ConnectWrapper<ConnType, RespType>>(ConnectWrapper<ConnType, RespType>(-1, nullptr, nullptr));
        }
        int connectorIdx = m_freeConnInfos.front();
        m_freeConnInfos.pop_front();
        M_SYLAR_ASSERT2(connectorIdx >= 0 && connectorIdx < m_connectorCount, "connector is out of range");
        ++m_busyConnCount;
        if(m_busyConnCount == m_connectorCount) {
            m_state = FULL;
        }
        return std::make_shared<ConnectWrapper<ConnType, RespType>>(ConnectWrapper<ConnType, RespType>(connectorIdx, m_connectors[connectorIdx], this));
    }
    M_SYLAR_LOG_WARN(gdb_logger) << "unknown DBManager State: " << m_state;
    return std::make_shared<ConnectWrapper<ConnType, RespType>>(ConnectWrapper<ConnType, RespType>(-1, nullptr, nullptr));
}




template<DBConnectType ConnType, typename RespType>
int DBPool<ConnType, RespType>::returnConn(const int conn_idx, const bool isTimeOut) {
    std::unique_lock<std::shared_mutex> w_lock(m_mutex);
    if(!checkRunState() || (conn_idx < 0 || conn_idx >= m_maxConnector)) {
        return -1;
    }

    if(isTimeOut) {
        w_lock.unlock();
        auto new_connect = std::make_shared<ConnType>();
        int rt = new_connect->connect(m_connectorBaseInfo);
        if(rt == -1) {
            M_SYLAR_LOG_ERROR(gdb_logger) << "failed to connect to database: " << m_connectorBaseInfo->host;
            --m_connectorCount;
            --m_busyConnCount;
            return -1;
        }
        m_connectors[conn_idx] = new_connect;
        w_lock.lock();
    }
    // 资源归还
    m_freeConnInfos.push_back(conn_idx);
    --m_busyConnCount;
    if(m_state == FULL) {
        m_state = READY;
    }
    M_SYLAR_LOG_DEBUG(gdb_logger) << "max connector num: " << m_maxConnector
            << " curr connector count: " << m_connectorCount
            << " busy connector count: " << m_busyConnCount;

    // 唤醒部分等待者
    w_lock.unlock();
    tickle();
    return 0;
}




template<DBConnectType ConnType, typename RespType>
int DBPool<ConnType, RespType>::expand() {
    if(!checkRunState()) {
        M_SYLAR_LOG_ERROR(gdb_logger) << "failed to expand connector pool, invalid pool state";
    }

    if(m_maxConnector == m_connectorCount) {
        M_SYLAR_ASSERT2(m_connectorCount == m_connectors.size(), "connector count is not equals to connectors.size(), bad State");
        return 0;   // 已到连接最大值
    }


    std::unique_lock w_lock(m_mutex);
    // 计算增量
    int increaseNum = m_increaseNum;
    const int temp = m_maxConnector - m_connectorCount;
    increaseNum =  (increaseNum < temp) ? increaseNum : temp;

    // 扩容
    int rt = 0;
    int max_idx = 0;
    for(int i = m_connectorCount; i < m_connectorCount + increaseNum && !rt; i++) {
        rt = m_connectors[i]->connect(m_connectorBaseInfo);
        max_idx = i;
        m_freeConnInfos.push_back({i});
    }
    if(rt) {    // 错误检查.
        M_SYLAR_LOG_ERROR(gdb_logger) << "failed to expand MySQLPoolManager, new connect failed.";
        m_connectorCount = max_idx;
        return -1;
    }

    // 信息设置
    m_connectorCount = m_connectorCount + increaseNum;
    if(m_connectorCount > m_busyConnCount) {
        m_state = State::READY;
    }
    return 0;
}

}
