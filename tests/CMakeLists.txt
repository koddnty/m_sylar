include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(tests test_main.cpp)
add_dependencies(tests sylar)
target_link_libraries(tests Catch2::Catch2WithMain sylar ${LIB_LIB})




##websocket 测试
add_executable(test_websocket server/test_websocket.cc)
add_dependencies(test_websocket sylar)
target_link_libraries(test_websocket sylar ${LIB_LIB})


# config 测试
add_executable(test_config test_config2.cc)
add_dependencies(test_config sylar)
target_link_libraries(test_config sylar ${LIB_LIB})


# redis 测试
add_executable(redis_test sql/redis_test.cc)
add_dependencies(redis_test sylar)
target_link_libraries(redis_test sylar ${LIB_LIB})

# mysql测试
add_executable(poolmysql_test sql/poolmysql_test.cc)
add_dependencies(poolmysql_test sylar)
target_link_libraries(poolmysql_test sylar ${LIB_LIB})

add_executable(mysql_conntest sql/mysql_test.cc)
add_dependencies(mysql_conntest sylar)
target_link_libraries(mysql_conntest sylar ${LIB_LIB})

add_executable(poolredis_test sql/poolredis_test.cc)
add_dependencies(poolredis_test sylar)
target_link_libraries(poolredis_test sylar ${LIB_LIB})

add_executable(taskCoro_test lab_test/taskCoro_test.cc)
add_dependencies(taskCoro_test sylar)
target_link_libraries(taskCoro_test sylar ${LIB_LIB})

add_executable(task_test lab_test/task_test.cc)
add_dependencies(task_test sylar)
target_link_libraries(task_test sylar ${LIB_LIB})

add_executable(test test.cc)
add_dependencies(test sylar)
target_link_libraries(test sylar ${LIB_LIB})

# TODO 线程池测试（待更新测试）
# add_executable(test_thread test_thread.cc)
# add_dependencies(test_thread sylar )
# target_link_libraries(test_thread sylar ${LIB_LIB})

add_executable(test_util test_util.cc)
add_dependencies(test_util sylar )
target_link_libraries(test_util sylar ${LIB_LIB})


add_executable(test_scheduler scheduler.cc)
add_dependencies(test_scheduler sylar )
target_link_libraries(test_scheduler sylar ${LIB_LIB})

add_executable(ock_test ock_test.cc)
add_dependencies(ock_test sylar )
target_link_libraries(ock_test sylar ${LIB_LIB})

# 定时器模块测试
# add_executable(test_self_timer test_self_timer.cc)
# add_dependencies(test_self_timer sylar )
# target_link_libraries(test_self_timer sylar ${LIB_LIB})

add_executable(address_test address_test.cc)
add_dependencies(address_test sylar)
target_link_libraries(address_test sylar ${LIB_LIB})

# socket模块测试(待测试)
# add_executable(socket_test test_socket.cc)
# add_dependencies(socket_test sylar)
# target_link_libraries(socket_test sylar ${LIB_LIB})

# TODO 更新http模块测试（待更新测试）
# add_executable(http_test http_test.cc)
# add_dependencies(http_test sylar)
# target_link_libraries(http_test sylar ${LIB_LIB})


add_executable(tcpServer_test tcpServer_test.cc)
add_dependencies(tcpServer_test sylar)
target_link_libraries(tcpServer_test sylar ${LIB_LIB})

add_executable(httpServer_test httpServer_test.cc)
add_dependencies(httpServer_test sylar)
target_link_libraries(httpServer_test sylar ${LIB_LIB})

add_executable(performance_test_scheduler performance_tests/scheduler_exchange.cc)
add_dependencies(performance_test_scheduler sylar)
target_link_libraries(performance_test_scheduler sylar ${LIB_LIB})

add_executable(scheduleCoro20_test lab_test/scheduleCoro20_test.cc)
add_dependencies(scheduleCoro20_test sylar)
target_link_libraries(scheduleCoro20_test sylar ${LIB_LIB})

add_executable(iomanagerCoro20_test lab_test/iomanagerCoro20_test.cc)
add_dependencies(iomanagerCoro20_test sylar)
target_link_libraries(iomanagerCoro20_test sylar ${LIB_LIB})

