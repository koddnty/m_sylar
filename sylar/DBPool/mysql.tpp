





namespace m_sylar {

// StmtParams ------------------------------------------------------------
template <typename... Args>
template <size_t I, typename T>
void StmtParams<Args...>::bindOne(T& value) {                // 绑定一个[i]值
    MYSQL_BIND& b = binds_[I];

    if constexpr (is_optional_v<T>) {
        // optional<U>：有值就按 U 绑定，同时挂上 is_null 指针
        nulls_[I] = value.has_value() ? 0 : 1;
        b.is_null = &nulls_[I];
        if (value.has_value()) bindOne<I>(*value);
    } else if constexpr (std::is_same_v<T, std::string>) {      // string类型绑定
        b.buffer_type   = MYSQL_TYPE_STRING;
        b.buffer        = value.data();
        b.buffer_length = static_cast<unsigned long>(value.size());
        lengths_[I]     = static_cast<unsigned long>(value.size());
        b.length        = &lengths_[I];
    } else if constexpr (std::is_same_v<T, bool>) {         // bool类型绑定
        b.buffer_type = MYSQL_TYPE_TINY;
        b.buffer      = &value;
    } else if constexpr (std::is_integral_v<T>) {               // 数字类型绑定
        b.buffer_type = (sizeof(T) == 8) ? MYSQL_TYPE_LONGLONG : MYSQL_TYPE_LONG;
        b.buffer      = &value;
        b.is_unsigned = std::is_unsigned_v<T>;
    } else if constexpr (std::is_floating_point_v<T>) {             // 浮点数绑定
        b.buffer_type = std::is_same_v<T, float> ? MYSQL_TYPE_FLOAT : MYSQL_TYPE_DOUBLE;
        b.buffer      = &value;
    } else {                        // 其他数据类型
        static_assert(!sizeof(T), "unsupported param type");
    }
}







// MySQLStmt ------------------------------------------------------------
template <typename... Cols>
MySQLStmt<Cols...>::MySQLStmt(ConnectWrapper<MySQLConn, MySQLResp>::ptr  conn_wrapper) : m_conn_wrapper(std::move(conn_wrapper)){
    m_stmt = mysql_stmt_init(m_conn_wrapper->getConnector()->getMYSQL());
    m_state = State::INIT;
}


template <typename... Cols>
inline Task<IOState> MySQLStmt<Cols...>::co_prepare(const std::string& query) {
    if (m_state != State::INIT) {
        co_return IOState::FAILED;
    }

    // prepare处理
    int ret = 0;
    auto io_state = IOState::SUCCESS;
    int status = mysql_stmt_prepare_start(&ret, m_stmt, query.c_str(), query.length());
    while (status) {
        io_state = co_await MysqlAwaiter(m_conn_wrapper->getConnector()->getMYSQL(), status, MYSQL_QUERY_TIMEOUT);
        if(io_state != IOState::SUCCESS) {
            M_SYLAR_LOG_ERROR(gmq_logger)   << "(mysql IO) failed to execute mysql_stmt_execute_cont, error:"
                                            << mysql_error(m_conn_wrapper->getConnector()->getMYSQL());
            co_return io_state;
        }
        status = mysql_stmt_prepare_cont(&ret, m_stmt, status);
    }

    // 返回值检查
    if (ret) {
        M_SYLAR_LOG_ERROR(gmq_logger)   << "(mysql RET) failed to execute mysql_stmt_execute_cont, error:"
                                        << mysql_error(m_conn_wrapper->getConnector()->getMYSQL());
        co_return IOState::FAILED;
    }

    // 处理状态信息
    m_state = State::PREPARE;
    co_return IOState::SUCCESS;
}


template <typename... Args>
Task<IOState> MySQLStmt<Args...>::co_bindAndExecute(Args&&... params) {
    if (m_state != State::PREPARE ) {
        co_return IOState::FAILED;
    }

    // 绑定
    // ReSharper disable once CppTooWideScopeInitStatement
    auto stmt_params = makeParams(std::forward<Args>(params)...);
    if (mysql_stmt_bind_param(m_stmt, stmt_params.data())) {
        M_SYLAR_LOG_ERROR(gmq_logger)   << "failed to bind mysql_stmt_bind_param, error: "
                                        << mysql_error(m_conn_wrapper->getConnector()->getMYSQL());
        co_return IOState::FAILED;
    }
    // 绑定返回位置
    if(mysql_stmt_bind_result(m_stmt, m_result.getCacheRow().data())) {
        M_SYLAR_LOG_ERROR(gmq_logger) << "failed to bind mysql_stmt_bind_result, error: "
                                        << mysql_error(m_conn_wrapper->getConnector()->getMYSQL());
        co_return IOState::FAILED;
    }

    // 执行
    int ret = 0;
    auto io_state = IOState::SUCCESS;
    int status = mysql_stmt_execute_start(&ret, m_stmt);
    while (status) {
        io_state = co_await MysqlAwaiter(m_conn_wrapper->getConnector()->getMYSQL(), status, MYSQL_QUERY_TIMEOUT);
        if(io_state != IOState::SUCCESS) {
            M_SYLAR_LOG_ERROR(gmq_logger)   << "(mysql IO) failed to execute mysql_stmt_execute_cont, error:"
                                            << mysql_error(m_conn_wrapper->getConnector()->getMYSQL());
            co_return io_state;
        }
        status = mysql_stmt_execute_cont(&ret, m_stmt, status);
    }

    // 返回值检查
    if (ret) {
        M_SYLAR_LOG_ERROR(gmq_logger)   << "(mysql RET) failed to execute mysql_stmt_execute_cont, error:"
                                        << mysql_error(m_conn_wrapper->getConnector()->getMYSQL());
        co_return IOState::FAILED;
    }

    // 状态更新
    m_state = State::EXECUTE;
    co_return IOState::SUCCESS;
}


template <typename... Args>
Task<IOState> MySQLStmt<Args...>::co_storeAll() {
    if (m_state != State::EXECUTE) {
        M_SYLAR_LOG_WARN(gmq_logger) << "bad Stmt state, expected: EXECUTE, curr : " << m_stmt;
        co_return IOState::FAILED;
    }

    // 获取数据
    auto io_state = IOState::SUCCESS;
    int ret = 0;
    int status = mysql_stmt_store_result_start(&ret, m_stmt);
    while (status) {
        io_state = co_await MysqlAwaiter(m_conn_wrapper->getConnector()->getMYSQL(), status, MYSQL_QUERY_TIMEOUT);
        if(io_state != IOState::SUCCESS) {
            M_SYLAR_LOG_ERROR(gmq_logger)   << "(mysql IO) failed to execute mysql_stmt_execute_cont, error:"
                                            << mysql_error(m_conn_wrapper->getConnector()->getMYSQL());
            co_return IOState::FAILED;
        }
        status = mysql_stmt_store_result_cont(&ret, m_stmt, status);
    }

    if (ret) {
        M_SYLAR_LOG_ERROR(gmq_logger) <<  "(mysql RET) failed to execute mysql_stmt_store_result_cont, error:"
                                        << mysql_error(m_conn_wrapper->getConnector()->getMYSQL());
        co_return IOState::FAILED;
    }

    m_state = State::STORE;       // 回到开始,可重新准备后继续查询
    co_return IOState::SUCCESS;
}


template <typename... Args>
Task<std::optional<std::tuple<Args...>>> MySQLStmt<Args...>::co_fetchNext() {
    if (m_state != State::EXECUTE && m_state != State::STORE) {
        M_SYLAR_LOG_WARN(gmq_logger) << "bad Stmt state, expected: EXECUTE, curr : " << m_stmt;
        co_return std::nullopt;
    }

    // 获取数据
    auto io_state = IOState::SUCCESS;
    int ret = 0;
    int status = mysql_stmt_fetch_start(&ret, m_stmt);
    while (status) {
        io_state = co_await MysqlAwaiter(m_conn_wrapper->getConnector()->getMYSQL(), status, MYSQL_QUERY_TIMEOUT);
        if(io_state != IOState::SUCCESS) {
            M_SYLAR_LOG_ERROR(gmq_logger)   << "(mysql IO) failed to fetch mysql_stmt_execute_cont, error:"
                                            << mysql_error(m_conn_wrapper->getConnector()->getMYSQL());
            co_return std::nullopt;
        }
        status = mysql_stmt_fetch_cont(&ret, m_stmt, status);
    }
    if (ret == MYSQL_NO_DATA) {
        m_state = State::FETCH;
        co_return std::nullopt;
    }
    if (ret) {
        M_SYLAR_LOG_ERROR(gmq_logger) <<  "(mysql RET) failed to fetch mysql_stmt_store_result_cont, error:"
                                        << mysql_error(m_conn_wrapper->getConnector()->getMYSQL());
        co_return std::nullopt;
    }

    // 状态更新
    co_return m_result.getCacheRow().getValues();
}


template <typename... Args>
Task<IOState> MySQLStmt<Args...>::co_fetchAll() {
    std::optional<std::tuple<Args...>> ret;
    while (true) {
        ret = co_await co_fetchNext();
        if (!ret.has_value()) {
            break;
        }
        m_result.append(ret.value());           // 存储数据
    }
    co_return (m_state == State::FETCH) ? IOState::SUCCESS : IOState::FAILED;
}

template<typename ...Cols>
Task<IOState> MySQLStmt<Cols...>::co_close() {
    auto io_state = IOState::SUCCESS;
    my_bool ret = 0;
    int status = mysql_stmt_close_start(&ret, m_stmt);
    while (status) {
        io_state = co_await MysqlAwaiter(m_conn_wrapper->getConnector()->getMYSQL(), status, MYSQL_QUERY_TIMEOUT);
        if(io_state != IOState::SUCCESS) {
            M_SYLAR_LOG_ERROR(gmq_logger)   << "(mysql IO) failed to close mysql_stmt_close_cont, error:"
                                            << mysql_error(m_conn_wrapper->getConnector()->getMYSQL());
            co_return IOState::FAILED;
        }
        status = mysql_stmt_close_cont(&ret, m_stmt, status);
    }

    if (ret != 0) {
        M_SYLAR_LOG_ERROR(gmq_logger) <<  "(mysql RET) failed to close mysql_stmt_store_close_cont, error:"
                                        << mysql_error(m_conn_wrapper->getConnector()->getMYSQL());
        co_return IOState::FAILED;
    }

    // 成功后归还连接并切断与连接池联系,清空其他模块资源
    m_stmt = nullptr;
    m_conn_wrapper.reset();
    co_return IOState::SUCCESS;
}


}