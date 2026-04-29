#include "DBPool/redis.h"

int main(void) {
    // The `redisContext` type represents the connection
    // to the Redis server. Here, we connect to the
    // default host and port.
    redisContext *c = redisConnect("127.0.0.1", 6379);

    // Check if the context is null or if a specific
    // error occurred.
    if (c == NULL || c->err) {
        if (c != NULL) {
            printf("Error: %s\n", c->errstr);
            // handle error
        } else {
            printf("Can't allocate redis context\n");
        }

        exit(1);
    }

    // Set a string key.
    redisReply *reply = (redisReply*)redisCommand(c, "SET name \"sylar\"");
    m_sylar::RedisResp resp (reply);
    printf("Reply: %s\n", resp.asString().c_str()); // >>> Reply: OK

    // Get the key we have just stored.
    reply = (redisReply*)redisCommand(c, "GET name");
    m_sylar::RedisResp resp2 {reply};
    
    printf("Reply: %s\n", resp2.asString().c_str()); // >>> Reply: bar

    // Close the connection.
    redisFree(c);
    return 0;
}