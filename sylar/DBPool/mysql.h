#pragma once
#include <iostream>
#include <memory>
#include <mariadb/mysql.h>
#include <map>
#include <vector>
#include "basic/log.h"
#include "basic/timer/timer.hpp"
#include "coroutine/corobase.h"
#include "database.h"


namespace m_sylar{

#define MYSQL_CONNECT_CREASE_SPEED 10           // 连接池单次扩容容量
#define MYSQL_QUERY_TIMEOUT 30000             // 最大语句查询超时时间

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

	int getColCount() const {return m_colCount;}
	int getRowCount() const {return m_rowCount;}
    IOState getState() const {return m_state;}
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

public:
int connect(const std::string& host,
						const std::string& user,
						const std::string& passwd,
						const std::string& db,
						unsigned int port,
						unsigned long clientflag);

Task<MySQLResp::ptr> executeQuery(const std::string& query);

MYSQL* getMYSQL() const {return m_mysql; }


private:
	MYSQL* m_mysql {nullptr};
	State m_state {State::INIT};
};



class MySQLPoolManager : public DBPool<MySQLConn, MySQLResp>{
public:
	using ptr = std::shared_ptr<MySQLPoolManager>;

	MySQLPoolManager(int min_conn, int max_conn);
	~MySQLPoolManager() override = default;

	struct MySQLConnectInfo {
		std::string host;
		std::string user;
		std::string passwd;
		std::string db;
		unsigned int port;
		unsigned long clientflag;
	};

	Task<MySQLResp::ptr> executeQuery(const std::string& query) override;

	int init(const std::string& host,
										const std::string& user,
										const std::string& passwd,
										const std::string& db,
										unsigned int port,
										unsigned long clientflag);
	// void close();           // 涉及fd的关闭，请勿在绑定的iomanager结束前调用，否则可能会造成其他错误，此函数为阻塞函数

	int registeConnCb(std::function<void()> cb) override;        // 用于awaiter的回调
	int tickle() override;                                       // 有新连接时的回调, 用于连接耗尽时阻塞控制, 若状态为关闭则全部tickle.

	// bool checkRunState();           // 若当前连接池处于正常可运行状体则返回true;



protected:
	int borrowOneConn() override;               // 线程不安全, 返回空闲连接索引
	int returnConn(int free_idx, bool isTimeOut) override;              // 线程不安全
	int expand() override;                               // 线程安全

private:
	std::shared_mutex m_ConnectPoolMutex;               // 连接获取等使用锁
	MySQLConnectInfo m_connectorBaseInfo;               // 连接基本信息， 用于扩容。
};

}