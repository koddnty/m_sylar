#include <hiredis/hiredis.h>
#include <memory>
#include "database.h"


namespace m_sylar {


class RedisResp {
public:
    using ptr = std::shared_ptr<RedisResp>;
    RedisResp(redisReply* reply, bool is_child = false);
    ~RedisResp();

    int getType();      /* REDIS_REPLY_*, 类型与hiredis提供一致 */
    long long asInt(); 
    double asDouble();
    std::string asString();
    const std::vector<RedisResp::ptr>& asArray();

private:
    redisReply* m_reply;

    bool m_is_arry_inited = false;
    bool m_is_child = false;            // 若为true,则析构时不会free数据
    std::vector<RedisResp::ptr> m_array = {};
};

// class RedisConn {
// public: 
//     RedisConn();
//     ~RedisConn();

//     int connect(const std::string& ip, int port);
//     int connect(std::string ip, int port);

//     RedisResp::ptr executeQuery();

// };


// class RedisPoolManager : public DBPool<RedisConn, RedisResp>{
// public:


// };
}