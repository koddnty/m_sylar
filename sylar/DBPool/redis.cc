#include <hiredis/read.h>
#include <memory>
#include <hiredis/hiredis.h>
#include "redis.h"
#include "basic/macro.h"
#include "database.h"


namespace m_sylar {

RedisResp::RedisResp(redisReply* reply, bool is_child){
    M_SYLAR_ASSERT2(reply, "reply is nullptr");
    m_reply = reply;
    m_is_child = is_child;
}

RedisResp::~RedisResp(){
    if(!m_is_child) {
        freeReplyObject(m_reply);
    }
}

int RedisResp::getType(){
    return m_reply->type;
}

long long RedisResp::asInt(){
    if(m_reply->type == REDIS_REPLY_INTEGER) {
        return m_reply->integer;
    }
    else {
        throw std::bad_cast();
    }
}

double RedisResp::asDouble() {
    if(m_reply->type == REDIS_REPLY_DOUBLE) {
        return m_reply->dval;
    }
    else if(m_reply->type == REDIS_REPLY_INTEGER) {
        return m_reply->integer;
    }
    else {
        throw std::bad_cast();
    }
}

std::string RedisResp::asString(){
    switch (m_reply->type) {
        case REDIS_REPLY_ERROR:
        case REDIS_REPLY_STRING:
        case REDIS_REPLY_VERB:
        case REDIS_REPLY_DOUBLE:
        case REDIS_REPLY_STATUS:
        case REDIS_REPLY_BIGNUM:
            return std::string(m_reply->str, m_reply->len);
            break;
        case REDIS_REPLY_INTEGER:
            return std::to_string(m_reply->integer);
            break;
        default:
            std::cout << m_reply->type << std::endl;
            throw std::bad_cast();
            break;
    }
}

const std::vector<RedisResp::ptr>& RedisResp::asArray() {
    if(m_is_arry_inited) {
        return m_array;
    }

    m_array.resize(m_reply->elements);
    for(int i = 0; i < m_reply->elements; i++) {
        m_array[i] = std::make_shared<RedisResp>(m_reply->element[i], true);        // 析构不释放。
    }
    m_is_arry_inited = true;

    return m_array;
}



}