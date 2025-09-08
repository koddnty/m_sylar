cmake_minimum_required(VERSION 3.10)
project(m_sylar)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic  -fPIC -ggdb -std=c++17 -Wall -Wno-deprecated  -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")
# -O2
set(LIB_SRC
    sylar/log.cc
    sylar/until.cc
    sylar/config.cc
    )

add_library(sylar SHARED ${LIB_SRC})

add_executable(test tests/test.cc)
add_dependencies(test sylar)
target_link_libraries(test sylar)

add_executable(test_conf tests/test_config.cc)
add_dependencies(test_conf sylar)
target_link_libraries(test_conf sylar)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)