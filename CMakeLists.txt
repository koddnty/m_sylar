cmake_minimum_required(VERSION 3.10)
project(m_sylar)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
# 原
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -g -fPIC -ggdb -std=c++20 -Wall -Wno-deprecated  -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")




option(SYLAR_USE_CORO20 "Use C++20 coroutine implementation, OFF if NOT" ON)       # 底层协程实现配置

find_library(MARIADB_LIB mariadb /usr/local/lib/mariadb)
find_library(HIREDIS_LIB hiredis /usr/local/lib)

set ( LIB_LIB
    pthread
    dl
    ${MARIADB_LIB}
    ${HIREDIS_LIB}
)

# 基础模块
set(BASIC_SOURCES
    sylar/basic/log.cc
    sylar/basic/until.cc
    sylar/basic/config.cc
    sylar/basic/thread.cc
    sylar/basic/fdManager.cc
    sylar/basic/address.cc
    sylar/basic/socket.cc
    sylar/basic/tool.cc

)

# 协议模块
file(GLOB_RECURSE PROTOCOL_SOURCES_FIND 
    "sylar/protocol/websocket/*.cc"
    "sylar/protocol/websocket/*.c"
    "sylar/protocol/websocket/*.cpp"
)
set(PROTOCOL_SOURCES
    ${PROTOCOL_SOURCES_FIND}
)

set(WEBSCOKET_SOURCES
    sylar/server/websocket/wsserver.cc
)

# 网络服务 模块
set(HTTP_SOURCES
    sylar/server/common/session.cc
    sylar/server/tcp/tcpServer.cc
    sylar/server/http/httpServer.cc
    sylar/protocol/websocket/parser.cc
)


# cpp20Coroutine 协程模块
set(CORO20_SOURCES
    sylar/coroutine/coro20/fiber.cc
    sylar/coroutine/coro20/fdContext.cc
    sylar/coroutine/coro20/scheduler.cc
    sylar/coroutine/coro20/ioManager.cc
    sylar/coroutine/coro20/hook.cc
    sylar/coroutine/coro20/taskPool.cc
)

# 数据库连接池模

set(DBPOOL_SOURCES
    sylar/DBPool/mysql.cc
    sylar/DBPool/redis.cc
    sylar/DBPool/factory.cc
)

# 新模块测试模块
set(TEST_SOURCES
    sylar/basic/timer/timer.cc
    sylar/basic/timer/timer_task.cc
    sylar/basic/timer/timer_block.cc
)

# 选择模块

add_definitions(-DSYLAR_USE_CORO20)

set(COROUTINE_SOURCES ${CORO20_SOURCES})
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(sylar STATIC 
    ${BASIC_SOURCES}
    ${HTTP_SOURCES}
    ${COROUTINE_SOURCES}
    ${DBPOOL_SOURCES}
    ${PROTOCOL_SOURCES}
    ${WEBSCOKET_SOURCES}
    ${TEST_SOURCES}
    sylar/basic/timer/timer_task.hpp
)


# 1. 安装静态库
install(TARGETS sylar
    ARCHIVE DESTINATION lib  # 静态库(.a)安装到 /usr/local/lib
)

# 2. 安装头文件（整个目录）
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sylar/
    DESTINATION include/sylar
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.tpp" 
    PATTERN ".DS_Store" EXCLUDE
)

target_include_directories(sylar PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/sylar
    /usr/include/mysql-cppconn
)



set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)


add_subdirectory(${PROJECT_SOURCE_DIR}/tests)
