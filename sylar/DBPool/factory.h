#pragma once
#include "mysql.h"
#include "basic/log.h"
#include "database.h"
#include "mysql.h"
#include "redis.h"

namespace m_sylar
{

class DB {
public: 
    /**
        @brief 创建一个mysql连接池并返回抽象类指针
        @param is_public 默认为true,即应用到公共范围，通过getInstence获得MysqlPool连接池。若为false则需手动传递，自行管理 
     */
    static DBPool<MySQLConn, MySQLResp>::ptr createMysqlPool(int min_conn, int max_conn, bool is_public = true);

    class Mysql {   public:
        // 无实例将创建默认连接池，未初始化！
        static MySQLPoolManager::ptr getInstance();
    };


    /**
        @brief 创建一个redis连接池并返回抽象类指针
        @param is_public 默认为true,即应用到公共范围，通过getInstence获得MysqlPool连接池。若为false则需手动传递，自行管理 
     */
    static DBPool<RedisConn, RedisResp>::ptr createRedisPool(int min_conn, int max_conn, bool is_public = true);

    class Redis {   public:
        // 无实例将创建默认连接池，未初始化！
        static RedisPoolManager::ptr getInstance();
    };
};
}