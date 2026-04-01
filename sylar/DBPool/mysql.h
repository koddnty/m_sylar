#pragma once
#include <iostream>
#include <memory>
#include <mariadb/mysql.h>
#include "coroutine/corobase.h"
#include "basic/log.h"


namespace m_sylar{

#define MYSQL_CONNECT_CREASE_SPEED 10

// MYSQL awaiter, 仅通知，不执行
class MysqlAwaiter : public Awaiter<void>{
public: 
    MysqlAwaiter(MYSQL* mysql, int status);
    ~MysqlAwaiter();

    void on_suspend()override;
    void before_resume() override;

private:
    int m_fd = -1;
    FdContext::Event m_event {FdContext::NONE};
};


class MySQLResp{
public: 
    using ptr = std::shared_ptr<MySQLResp>;
    MySQLResp(MYSQL* mysql);
    MySQLResp() { m_state = false; m_respBody = nullptr; }
    ~MySQLResp();

    class Value{
    public:
        Value(char* data, int length);
        Value();
        std::string get();
        char* get(size_t& length);
        operator bool() {return m_isValid;}
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
        operator bool() {return m_row != NULL;}
    private:
        MYSQL_ROW m_row;
        int m_colCount;
        int m_colIdx;
        unsigned long* m_eleLength;
    };

public:
    Row nextRow();
    void resetRow();   // 把行索引设置为idx = 0;


private:
    MYSQL_RES* m_respBody {nullptr};
    int m_colCount = 0;
    bool m_state = true;
};


class MySQLDB{
public:
    using ptr = std::shared_ptr<MySQLDB>;

    enum State{
        READY = 0,
        READ = MYSQL_WAIT_READ,
        WRITE = MYSQL_WAIT_WRITE,
        ERROR = MYSQL_WAIT_EXCEPT,
        TIMO = MYSQL_WAIT_TIMEOUT,
        INIT = 16
    };
 
    MySQLDB();
    ~MySQLDB();

public:
int connect(const std::string& host,
                        const std::string& user,
                        const std::string& passwd,
                        const std::string& db,
                        unsigned int port,
                        unsigned long clientflag);

Task<MySQLResp::ptr> executeQuery(const std::string& query);

    

private:
    MYSQL* m_mysql {nullptr};
    State m_state {State::INIT};
    std::string name;
};



class MySQLPoolManager{
public:
    using ptr = std::shared_ptr<MySQLPoolManager>;

    MySQLPoolManager(int min_conn, int max_conn);
    ~MySQLPoolManager();

    enum State{
        INIT = 0,
        READY = 1,
        FULL = 2,
        CLOSED = 4,
        CLOSING = 8,
        ERROR = 16
    };

    struct MySQLConnectInfo {
        std::string host;
        std::string user;
        std::string passwd;
        std::string db;
        unsigned int port;
        unsigned long clientflag;
    };
    
    Task<MySQLResp::ptr> executeQuery(const std::string& query);

    int init(const std::string& host,
                                        const std::string& user,
                                        const std::string& passwd,
                                        const std::string& db,
                                        unsigned int port,
                                        unsigned long clientflag);
    void close();           // 涉及fd的关闭，请勿在绑定的iomanager结束前调用，否则可能会造成其他错误，此函数为阻塞函数

    int registeConnCb(std::function<void()> cb);        // 用于awaiter的回调
    int tickle();                                       // 有新连接时的回调, 用于连接耗尽时阻塞控制, 若状态为关闭则全部tickle.

    bool checkRunState();           // 若当前连接池处于正常可运行状体则返回true;



protected:
    int borrowOneConn();               // 线程不安全, 返回空闲连接索引
    int returnConnn(int free_idx);              // 线程不安全
    int expand();                               // 线程安全

private:
    std::shared_mutex m_ConnectPoolMutex;           // 连接获取等使用锁

    std::vector<MySQLDB::ptr> m_connectors;         // 所有连接
    std::list<int> m_freeConnInfos;                 // 空闲信息表[连接对应idx]
     
    std::list<std::function<void()>> m_waitConnCb;              // 有新连接可用时会唤醒其中一个任务，优先队列
    std::atomic<int> m_connectorCount = 0;                       // 当前连接数目
    std::atomic<int> m_busyConnCount = 0;
    int m_minConnector;
    int m_maxConnector;
    std::atomic<State> m_state;

    MySQLConnectInfo m_connectorBaseInfo;               // 连接基本信息， 用于扩容。
};

}