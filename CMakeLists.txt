cmake_minimum_required(VERSION 3.10)
project(m_sylar)

# include (cmake/utils.cmake)
# add_definitions(-D__FILE__="\"$(notdir $(abspath $<))\"")

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -g -fPIC -ggdb -std=c++17 -Wall -Wno-deprecated  -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set ( LIB_LIB
    pthread
    dl
    yaml-cpp::yaml-cpp
)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__FILENAME__='\"$(subst  ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U__FILE__ -D__FILE__='\"$(subst $(dir $<),,$<)\"'")
# add_compile_options(-U__FILE__)
# add_compile_options(-D__FILE__="\"$(notdir %)\"")

find_package(yaml-cpp REQUIRED)

# add_library(sylar SHARED ${LIB_SRC})

#  add source files
add_library(sylar STATIC 
    sylar/basic/log.cc
    sylar/basic/until.cc
    sylar/basic/config.cc
    sylar/basic/thread.cc
    sylar/basic/fiber.cc
    sylar/basic/scheduler.cc
    sylar/basic/ioManager.cc
    sylar/basic/self_timer.cc    
    sylar/basic/hook.cc
    sylar/basic/fdManager.cc
    sylar/basic/address.cc
    sylar/basic/socket.cc
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

target_include_directories(sylar PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/sylar
)

# include_directories(
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/sylar
# )


add_executable(test tests/test.cc)
add_dependencies(test sylar)
target_link_libraries(test sylar ${LIB_LIB})

add_executable(test_conf tests/test_config.cc)
add_dependencies(test_conf sylar)
target_link_libraries(test_conf sylar ${LIB_LIB})

add_executable(test_conf2 tests/test_config2.cc)
add_dependencies(test_conf2 sylar)
target_link_libraries(test_conf2 sylar ${LIB_LIB})

add_executable(test_yaml tests/test_yaml.cc)
add_dependencies(test_yaml sylar)
target_link_libraries(test_yaml sylar ${LIB_LIB})

add_executable(test_thread tests/test_thread.cc)
add_dependencies(test_thread sylar )
target_link_libraries(test_thread sylar ${LIB_LIB})

add_executable(test_util tests/test_util.cc)
add_dependencies(test_util sylar )
target_link_libraries(test_util sylar ${LIB_LIB})

# test_fiber
add_executable(test_fiber tests/test_fiber.cc)
add_dependencies(test_fiber sylar )
target_link_libraries(test_fiber sylar ${LIB_LIB})

add_executable(test_scheduler tests/scheduler.cc)
add_dependencies(test_scheduler sylar )
target_link_libraries(test_scheduler sylar ${LIB_LIB})

add_executable(ock_test tests/ock_test.cc)
add_dependencies(ock_test sylar )
target_link_libraries(ock_test sylar ${LIB_LIB})


add_executable(test_iomanager tests/test_iomanager.cc)
add_dependencies(test_iomanager sylar )
target_link_libraries(test_iomanager sylar ${LIB_LIB})


add_executable(test_self_timer tests/test_self_timer.cc)
# force_redefine_macro_for_sources(test_yaml)
add_dependencies(test_self_timer sylar )
target_link_libraries(test_self_timer sylar ${LIB_LIB})

add_executable(test_hook tests/test_hook.cc)
add_dependencies(test_hook sylar)
target_link_libraries(test_hook sylar ${LIB_LIB})

add_executable(address_test tests/address_test.cc)
add_dependencies(address_test sylar)
target_link_libraries(address_test sylar ${LIB_LIB})

add_executable(socket_test tests/test_socket.cc)
add_dependencies(socket_test sylar)
target_link_libraries(socket_test sylar ${LIB_LIB})

add_executable(http_test tests/http_test.cc)
add_dependencies(http_test sylar)
target_link_libraries(http_test sylar ${LIB_LIB})

add_executable(httpParser_test tests/httpParser_test.cc)
add_dependencies(httpParser_test sylar)
target_link_libraries(httpParser_test sylar ${LIB_LIB})

add_executable(tcpServer_test tests/tcpServer_test.cc)
add_dependencies(tcpServer_test sylar)
target_link_libraries(tcpServer_test sylar ${LIB_LIB})

add_executable(httpServer_test tests/httpServer_test.cc)
add_dependencies(httpServer_test sylar)
target_link_libraries(httpServer_test sylar ${LIB_LIB})



add_executable(performance_test_scheduler tests/performance_tests/scheduler_exchange.cc)
add_dependencies(performance_test_scheduler sylar)
target_link_libraries(performance_test_scheduler sylar ${LIB_LIB})

#/ test_scheduler
#add_executable(test_scheduler tests/test_scheduler.cc)
# force_redefine_macro_for_sources(test_yaml)
#add_dependencies(test_scheduler sylar )
#target_link_libraries(test_scheduler sylar ${LIB_LIB})


set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
