#pragma once
#include <iostream>
#include <memory>
#include <mariadb/mysql.h>
#include "coroutine/corobase.h"
#include "basic/log.h"


namespace m_sylar{

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


private:
    MYSQL_RES* m_respBody;
    int m_colCount = 0;
    MYSQL_ROW row;
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
MySQLDB& connect(const std::string& host,
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



class MySQLDBManager{
public:
    using ptr = std::shared_ptr<MySQLDBManager>;



private:
    std::list<MySQLDB::ptr> m_freePool;
    std::list<MySQLDB::ptr> m_usingPool;
};

}