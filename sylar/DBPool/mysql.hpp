#pragma once
#include <iostream>
#include <memory>
#include <mariadb/mysql.h>
#include <map>
#include <vector>
#include "basic/log.h"
#include "basic/timer/timer.hpp"
#include "coroutine/corobase.h"
#include "database.hpp"


namespace m_sylar{

#define MYSQL_CONNECT_CREASE_SPEED 10           // 连接池单次扩容容量
#define MYSQL_QUERY_TIMEOUT 30000             // 最大语句查询超时时间

static Logger::ptr gmq_logger = M_SYLAR_LOG_NAME("system");

// MYSQL awaiter, 仅通知，不执行
class MysqlAwaiter : public Awaiter<IOState>{
public: 
	MysqlAwaiter(MYSQL* mysql, int status, uint64_t timeOut);
	~MysqlAwaiter() override;

	void on_suspend()override;
	void before_resume() override;


private:
	int m_fd = -1;
	uint64_t m_timeout;
	FdContext::Event m_event {FdContext::NONE};
};


class MySQLResp{
public: 
	using ptr = std::shared_ptr<MySQLResp>;
	explicit MySQLResp(MYSQL* mysql, IOState state);            // 错误传参默认失败
	explicit MySQLResp() = default;
	~MySQLResp();

	class Value{
	public:
		Value(char* data, int length);
		Value();
		std::string get();
		char* get(size_t& length);
		explicit operator bool() const {return m_isValid;}
	private:
		char* m_data;
		bool m_isValid = true;
		int m_length;
	};

	class Row{      // 行
	public:
		Row(MYSQL_ROW row, int colCount, unsigned long* eleLength);
		~Row();
		Value nextValue();
		explicit operator bool() const {return m_row != nullptr;}
	private:
		MYSQL_ROW m_row;
		int m_colCount;
		int m_colIdx;
		unsigned long* m_eleLength;
	};

public:
	Row nextRow();
	void resetRow();            // 把行索引设置为idx = 0;
	int formatDate();           // 格式化数据到map后支持索引访问, 有额外储存开销

	[[nodiscard]] int getColCount() const {return m_colCount;}
	[[nodiscard]] int getRowCount() const {return m_rowCount;}
    [[nodiscard]] IOState getState() const {return m_state;}
    void setState(const IOState state) {m_state = state;}


public: 
	class ColProxy {
	public:
		ColProxy(std::vector<std::pair<char*, size_t>>& vec) : m_vec(&vec){
		}

		ColProxy() {
			m_vec = nullptr;
			m_state = false;
		}

		std::string operator[](int i);

	private:
		std::vector<std::pair<char*, size_t>>* m_vec;
		bool m_state = true;
	};

	ColProxy operator[](std::string fieldName);

public:
    Task<IOState> co_fetchAll();



private:
    IOState m_state {IOState::UNKNOWN};
    MYSQL* m_mysql {nullptr};
	MYSQL_RES* m_respBody {nullptr};
	int m_colCount {0};
	int m_rowCount {0};
	std::map<std::string, std::vector<std::pair<char*, size_t>>> m_respMapData {};
};













// 数据库连接信息结构体   ------------------------------------------------------------
class MySQLConnectInfo : public ConnectInfoBase {
public:
    using ptr = std::shared_ptr<MySQLConnectInfo>;
    std::string user;
    std::string passwd;
    std::string db;
    unsigned long client_flag;
};

// template <typename... Args>
template<typename... Cols>
class MySQLStmt;
// mysql连接
class MySQLConn{
public:
	using ptr = std::shared_ptr<MySQLConn>;
	enum State{
		READY = 0,
		READ = MYSQL_WAIT_READ,
		WRITE = MYSQL_WAIT_WRITE,
		ERROR = MYSQL_WAIT_EXCEPT,
		TIMO = MYSQL_WAIT_TIMEOUT,
		INIT = 16
	};

	MySQLConn();
	~MySQLConn();



    int connect(ConnectInfoBase::ptr info);
    Task<MySQLResp::ptr> executeQuery(const std::string& query);
    [[nodiscard]] MYSQL* getMYSQL() const {return m_mysql; }


private:
	MYSQL* m_mysql {nullptr};
	State m_state {State::INIT};
};









// 数据库参数化查询封装   ------------------------------------------------------------
template <typename T>
struct is_optional : std::false_type {};
template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};
template <typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;


// 类型(到MYSQLBIND)地址转换, MariaDB 将数据写入 value，将 NULL 标记写入 is_null
template <size_t N>
struct Text {
    std::array<char, N> buf{'\0'};
    unsigned long length = 0;   // fetch 之后,这里是实际长度
    my_bool is_null = 0;        // fetch 之后,这一列是否是 NULL
    my_bool error = 0;          // fetch 之后,是否发生了截断(超过 N)

    std::string_view view() const { return {buf.data(), length}; }
};

template<size_t N>      // text转换到string类型
std::string TextToString(const Text<N>& text) {
    if (text.is_null) {return {}; }
    std::string output(text.buf.data(), text.length());
    return output;
}
template<size_t N>      // string转换到text类型
Text<N> StringToText(const std::string& str) {
    M_SYLAR_ASSERT2(N, "N(length of a Text) is zero!");
    Text<N> text;
    if (str.length() > N - 1) {
        text.error = 1;
    }
    const size_t len = N - 1 > str.length() ? str.length() : N - 1;
    std::memcpy(text.buf.data(), str.data(), len);
    text.buf[len] = '\0';
    text.is_null = 0;
    text.length = len;
    return text;
}

template<typename T>
void TtoBind(T& value, my_bool& is_null, MYSQL_BIND& b) {
    b.is_null = &is_null;
    if constexpr (std::is_same_v<T, bool>) {
        b.buffer_type = MYSQL_TYPE_TINY;
        b.buffer      = &value;
    } else if constexpr (std::is_integral_v<T>) {
        b.buffer_type = (sizeof(T) == 8) ? MYSQL_TYPE_LONGLONG : MYSQL_TYPE_LONG;
        b.buffer      = &value;
        b.is_unsigned = std::is_unsigned_v<T>;
    } else if constexpr (std::is_floating_point_v<T>) {
        b.buffer_type = std::is_same_v<T, float> ? MYSQL_TYPE_FLOAT : MYSQL_TYPE_DOUBLE;
        b.buffer      = &value;
    } else {
        static_assert(!sizeof(T), "unsupported result type");
    }
}
template <size_t N>     // string特化
void TtoBind(Text<N>& t, my_bool& is_null, MYSQL_BIND& b) {
    b.buffer_type   = MYSQL_TYPE_STRING;
    b.buffer        = t.buf.data();
    b.buffer_length = N;
    b.length        = &t.length;
    b.is_null       = &t.is_null;
    b.error         = &t.error;
}







// 封装绑定的参数
template <typename... Args>
class StmtParams {
public:
    using ptr = std::shared_ptr<StmtParams>;
    explicit StmtParams(Args... args) : storage_(std::move(args)...) {
        std::memset(binds_.data(), 0, sizeof(MYSQL_BIND) * sizeof...(Args));        // 内存清空
        bindAll(std::index_sequence_for<Args...>{});                // 存储所有绑定信息
    }

    // 禁止拷贝/移动：buffer 指针指向 storage_ 内部,一旦搬家就悬空
    StmtParams(const StmtParams&) = delete;
    StmtParams(StmtParams&&) = delete;

    MYSQL_BIND* data() { return binds_.data(); }
    static constexpr size_t size() { return sizeof...(Args); }

private:
    template <size_t... Is>
    void bindAll(std::index_sequence<Is...>) {          // 拆包,得到各种将要绑定的值
        (bindOne<Is>(std::get<Is>(storage_)), ...);
    }

    template <size_t I, typename T>
    void bindOne(T& value);

    std::tuple<Args...> storage_;
    std::array<MYSQL_BIND, sizeof...(Args)> binds_{};
    std::array<unsigned long, sizeof...(Args)> lengths_{};
    std::array<my_bool, sizeof...(Args)> nulls_{};
};

template <typename... Args>
auto makeParams(Args&&... args) {
    return StmtParams<std::decay_t<Args>...>(std::forward<Args>(args)...);      // 去除修饰并转发到构造函数,拷贝数据
}



template <typename... Args>
class StmtResultRow {
public:
    StmtResultRow() {
        std::memset(binds_.data(), 0, sizeof(MYSQL_BIND) * sizeof...(Args));
        bindAll(std::index_sequence_for<Args...>{});
    }

    StmtResultRow(const StmtResultRow&) = delete;
    StmtResultRow(StmtResultRow&&) = delete;

    MYSQL_BIND* data() { return binds_.data(); }

    // 取出第 I 个结果变量的引用,fetch 之后调用
    template <size_t I>
    auto& get() { return std::get<I>(storage_); }

    std::tuple<Args...> getValues() const {
        return storage_;
    }

private:
    template <size_t... Is>
    void bindAll(std::index_sequence<Is...>) {
        (bindOne<Is>(std::get<Is>(storage_)), ...);
    }

    // 针对 Text<N> 的特殊处理
    template <size_t I, size_t N>
    void bindOne(Text<N>& t) {
        MYSQL_BIND& b = binds_[I];
        b.buffer_type   = MYSQL_TYPE_STRING;
        b.buffer        = t.buf.data();
        b.buffer_length = N;
        b.length        = &t.length;
        b.is_null       = &t.is_null;
        b.error         = &t.error;
    }

    // 针对普通数值类型的处理,跟 StmtParams 里的 bindOne 几乎一样
    template <size_t I, typename T>
    void bindOne(T& value) {
        MYSQL_BIND& b = binds_[I];
        b.is_null  = &nulls_[I];
        if constexpr (std::is_same_v<T, bool>) {
            b.buffer_type = MYSQL_TYPE_TINY;
            b.buffer      = &value;
        } else if constexpr (std::is_integral_v<T>) {
            b.buffer_type = (sizeof(T) == 8) ? MYSQL_TYPE_LONGLONG : MYSQL_TYPE_LONG;
            b.buffer      = &value;
            b.is_unsigned = std::is_unsigned_v<T>;
        } else if constexpr (std::is_floating_point_v<T>) {
            b.buffer_type = std::is_same_v<T, float> ? MYSQL_TYPE_FLOAT : MYSQL_TYPE_DOUBLE;
            b.buffer      = &value;
        } else {
            static_assert(!sizeof(T), "unsupported result type");
        }
    }

    std::tuple<Args...> storage_{};
    std::array<MYSQL_BIND, sizeof...(Args)> binds_{};
    std::array<my_bool, sizeof...(Args)> nulls_{};
};


template <typename... Cols>
class StmtResult {
public:
    using ptr = std::shared_ptr<StmtResult>;
    explicit StmtResult() = default;
    ~StmtResult() = default;

    void addRow(const StmtResultRow<Cols...>& row) {
        m_rows.emplace_back(row);
    }

    const std::vector<std::tuple<Cols...>>& getAll() const {return m_rows;};
    StmtResultRow<Cols...> getCacheRow() const {return m_cache;}

    const StmtResultRow<Cols...>& operator[](size_t i) const {return m_rows[i];};

    template<size_t C>
    auto& get(size_t R) {return std::get<C>(m_rows[R]);}

    [[nodiscard]] size_t size() const {return m_rows.size();};

    void append(std::tuple<Cols...>&& row) { m_rows.emplace_back(std::move(row));}

private:
    StmtResultRow<Cols...> m_cache;
    std::vector<std::tuple<Cols...>> m_rows;
};





/**
 *  @brief 对mariaDB stmt查询进行cpp风格的封装,内部包含普通连接wrapper进行连接资源管理,获取普通连接后init构建stmt连接并执行相关操作
 *
 */
template<typename... Args>
class MySQLStmt {
public:
    using ptr = std::shared_ptr<MySQLStmt>;

    enum class State {
        INIT,
        PREPARE,
        BIND,
        EXECUTE,
        STORE,
        FETCH,
        CLOSED
    };
    inline explicit MySQLStmt(std::shared_ptr<ConnectWrapper<MySQLConn, MySQLResp>> conn_wrapper);            // 错误传参默认失败
    inline ~MySQLStmt() = default;

    Task<IOState> co_prepare(const std::string& query);            // 准备查询语句

    Task<IOState> co_bindAndExecute(Args&&... params);               // 绑定参数并执行

    Task<IOState> co_storeAll();                // 获取所有数据

    Task<std::optional<std::tuple<Args...>>> co_fetchNext();                // 获取下一行

    Task<IOState> co_fetchAll();                // 把数据并放到用户内存(m_result)

    Task<IOState> co_close();           // 关闭stmt


private:
    ConnectWrapper<MySQLConn, MySQLResp>::ptr m_conn_wrapper;
    MYSQL_STMT* m_stmt {nullptr};
    std::atomic<State> m_state {State::INIT};           // 存储当前应当执行的操作
    StmtResult<Args...> m_result;           // 数据存储位置
};








// 数据库连接池管理 ------------------------------------------------------------
class MySQLPoolManager : public DBPool<MySQLConn, MySQLResp>{
public:
	using ptr = std::shared_ptr<MySQLPoolManager>;

	MySQLPoolManager(int min_conn, int max_conn);
    ~MySQLPoolManager() override = default;



	Task<MySQLResp::ptr> executeQuery(const std::string& query);



	int init(const std::string& host,
										const std::string& user,
										const std::string& passwd,
										const std::string& db,
										unsigned int port,
										unsigned long client_flag);
	// void close();           // 涉及fd的关闭，请勿在绑定的iomanager结束前调用，否则可能会造成其他错误，此函数为阻塞函数


	// bool checkRunState();           // 若当前连接池处于正常可运行状体则返回true;




private:
	std::shared_mutex m_ConnectPoolMutex;               // 连接获取等使用锁
};

}


#include "mysql.tpp"