cmake_minimum_required(VERSION 3.10)
project(m_sylar)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
# 原
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -g -fPIC -ggdb -std=c++20 -Wall -Wno-deprecated  -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")




option(SYLAR_USE_CORO20 "Use C++20 coroutine implementation, OFF if NOT" ON)       # 底层协程实现配置

find_package(yaml-cpp REQUIRED)
find_library(MARIADB_LIB mariadb /usr/local/lib/mariadb)
find_library(HIREDIS_LIB hiredis /usr/local/lib)

set ( LIB_LIB
    pthread
    dl
    yaml-cpp::yaml-cpp
    ${MARIADB_LIB}
    ${HIREDIS_LIB}
)

# 基础模块
set(BASIC_SOURCES
    sylar/basic/log.cc
    sylar/basic/until.cc
    sylar/basic/config.cc
    sylar/basic/thread.cc
    sylar/basic/self_timer.cc
    sylar/basic/fdManager.cc
    sylar/basic/address.cc
    sylar/basic/socket.cc
)

# HTTP 模块
set(HTTP_SOURCES
    sylar/http/http.cc
    sylar/http/http11_parser.rl.cc
    sylar/http/httpclient_parser.rl.cc
    sylar/http/bstr/bsafe.c
    sylar/http/bstr/bstraux.c
    sylar/http/bstr/bstrlib.c
    sylar/http/dbg.cc
    sylar/http/httpParser.cc
    sylar/http/tcpServer.cc
    sylar/http/httpServer.cc
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
)



target_include_directories(sylar PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/sylar
    /usr/include/mysql-cppconn
)

# include_directories(
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/sylar
# )


# redis 测试
add_executable(redis_test tests/sql/redis_test.cc)
add_dependencies(redis_test sylar)
target_link_libraries(redis_test sylar ${LIB_LIB})

# mysql测试
add_executable(poolmysql_test tests/sql/poolmysql_test.cc)
add_dependencies(poolmysql_test sylar)
target_link_libraries(poolmysql_test sylar ${LIB_LIB})

add_executable(mysql_conntest tests/sql/mysql_test.cc)
add_dependencies(mysql_conntest sylar)
target_link_libraries(mysql_conntest sylar ${LIB_LIB})



# add_executable(taskCoro_test tests/lab_test/taskCoro_test.cc)
# add_dependencies(taskCoro_test sylar)
# target_link_libraries(taskCoro_test sylar ${LIB_LIB})

# add_executable(task_test tests/lab_test/task_test.cc)
# add_dependencies(task_test sylar)
# target_link_libraries(task_test sylar ${LIB_LIB})

# add_executable(test tests/test.cc)
# add_dependencies(test sylar)
# target_link_libraries(test sylar ${LIB_LIB})

# add_executable(test tests/test.cc)
# add_dependencies(test sylar)
# target_link_libraries(test sylar ${LIB_LIB})

# add_executable(test_conf tests/test_config.cc)
# add_dependencies(test_conf sylar)
# target_link_libraries(test_conf sylar ${LIB_LIB})

# add_executable(test_conf2 tests/test_config2.cc)
# add_dependencies(test_conf2 sylar)
# target_link_libraries(test_conf2 sylar ${LIB_LIB})

# add_executable(test_yaml tests/test_yaml.cc)
# add_dependencies(test_yaml sylar)
# target_link_libraries(test_yaml sylar ${LIB_LIB})

# add_executable(test_thread tests/test_thread.cc)
# add_dependencies(test_thread sylar )
# target_link_libraries(test_thread sylar ${LIB_LIB})

# add_executable(test_util tests/test_util.cc)
# add_dependencies(test_util sylar )
# target_link_libraries(test_util sylar ${LIB_LIB})

# # test_fiber
# add_executable(test_fiber tests/test_fiber.cc)
# add_dependencies(test_fiber sylar )
# target_link_libraries(test_fiber sylar ${LIB_LIB})

# add_executable(test_scheduler tests/scheduler.cc)
# add_dependencies(test_scheduler sylar )
# target_link_libraries(test_scheduler sylar ${LIB_LIB})

# add_executable(ock_test tests/ock_test.cc)
# add_dependencies(ock_test sylar )
# target_link_libraries(ock_test sylar ${LIB_LIB})


# add_executable(test_iomanager tests/test_iomanager.cc)
# add_dependencies(test_iomanager sylar )
# target_link_libraries(test_iomanager sylar ${LIB_LIB})


add_executable(test_self_timer tests/test_self_timer.cc)
# force_redefine_macro_for_sources(test_yaml)
add_dependencies(test_self_timer sylar )
target_link_libraries(test_self_timer sylar ${LIB_LIB})

# add_executable(test_hook tests/test_hook.cc)
# add_dependencies(test_hook sylar)
# target_link_libraries(test_hook sylar ${LIB_LIB})

# add_executable(address_test tests/address_test.cc)
# add_dependencies(address_test sylar)
# target_link_libraries(address_test sylar ${LIB_LIB})

# add_executable(socket_test tests/test_socket.cc)
# add_dependencies(socket_test sylar)
# target_link_libraries(socket_test sylar ${LIB_LIB})

# add_executable(http_test tests/http_test.cc)
# add_dependencies(http_test sylar)
# target_link_libraries(http_test sylar ${LIB_LIB})

add_executable(httpParser_test tests/httpParser_test.cc)
add_dependencies(httpParser_test sylar)
target_link_libraries(httpParser_test sylar ${LIB_LIB})

add_executable(tcpServer_test tests/tcpServer_test.cc)
add_dependencies(tcpServer_test sylar)
target_link_libraries(tcpServer_test sylar ${LIB_LIB})

add_executable(httpServer_test tests/httpServer_test.cc)
add_dependencies(httpServer_test sylar)
target_link_libraries(httpServer_test sylar ${LIB_LIB})

# add_executable(performance_test_scheduler tests/performance_tests/scheduler_exchange.cc)
# add_dependencies(performance_test_scheduler sylar)
# target_link_libraries(performance_test_scheduler sylar ${LIB_LIB})

add_executable(scheduleCoro20_test tests/lab_test/scheduleCoro20_test.cc)
add_dependencies(scheduleCoro20_test sylar)
target_link_libraries(scheduleCoro20_test sylar ${LIB_LIB})

add_executable(iomanagerCoro20_test tests/lab_test/iomanagerCoro20_test.cc)
add_dependencies(iomanagerCoro20_test sylar)
target_link_libraries(iomanagerCoro20_test sylar ${LIB_LIB})

#/ test_scheduler
#add_executable(test_scheduler tests/test_scheduler.cc)
# force_redefine_macro_for_sources(test_yaml)
#add_dependencies(test_scheduler sylar )
#target_link_libraries(test_scheduler sylar ${LIB_LIB})


set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
