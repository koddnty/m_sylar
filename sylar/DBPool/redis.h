#pragma once
#include "basic/log.h"
#include <hiredis/hiredis.h>
#include <memory>
#include "database.hpp"


namespace m_sylar {
#define REDIS_CONNECT_CREASE_SPEED 10           // 连接池单次扩容容量

class RedisResp {
public:
    using ptr = std::shared_ptr<RedisResp>;
    RedisResp(redisReply* mysql, IOState state = IOState::SUCCESS, bool is_child = false);
    ~RedisResp();

    int getType();      /* REDIS_REPLY_*, 类型与hiredis提供一致 */
    long long asInt(); 
    double asDouble();
    std::string asString();
    const std::vector<RedisResp::ptr>& asArray();

    IOState getState() const { return m_state; }
protected:
    RedisResp& setState(IOState state) { m_state = state; return *this; }

private:
    redisReply* m_reply;

    bool m_is_arry_inited = false;
    bool m_is_child = false;            // 若为true,则析构时不会free数据
    std::vector<RedisResp::ptr> m_array = {};
    IOState m_state = IOState::INIT;
};




class RedisConnectInfo : public ConnectInfoBase {
};


class RedisConn{
public: 
    using ptr = std::shared_ptr<RedisConn>;

    RedisConn();
    ~RedisConn();

    int connect(ConnectInfoBase::ptr info);

    RedisResp::ptr executeQuery(const std::string& query);

private:
    redisContext* m_connect {nullptr};
};


class RedisPoolManager : public DBPool<RedisConn, RedisResp>{
public:
    using ptr = std::shared_ptr<RedisPoolManager>;
    RedisPoolManager(int min_conn, int max_conn);
    ~RedisPoolManager();

    int init(const std::string& host, int port);     // 连接池初始化，连接到redis服务器


    Task<std::shared_ptr<RedisResp>> executeQuery(const std::string& query) override;


private:
    std::shared_mutex m_ConnectPoolMutex;        // 连接池锁
};

}