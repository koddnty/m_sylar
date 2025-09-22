cmake_minimum_required(VERSION 3.10)
project(m_sylar)

# include (cmake/utils.cmake)
# add_definitions(-D__FILE__="\"$(notdir $(abspath $<))\"")

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic  -fPIC -ggdb -std=c++17 -Wall -Wno-deprecated  -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

include_directories(.)

# -O2
set(LIB_SRC
    sylar/log.cc
    sylar/until.cc
    sylar/config.cc
    )

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__FILENAME__='\"$(subst  ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U__FILE__ -D__FILE__='\"$(subst $(dir $<),,$<)\"'")
# add_compile_options(-U__FILE__)
# add_compile_options(-D__FILE__="\"$(notdir %)\"")

find_package(yaml-cpp REQUIRED)

add_library(sylar SHARED ${LIB_SRC})
# force_redefine_macro_for_sources(sylar)
add_executable(test tests/test.cc)
add_dependencies(test sylar)
target_link_libraries(test sylar yaml-cpp::yaml-cpp)

add_executable(test_conf tests/test_config.cc)
# force_redefine_macro_for_sources(test_conf)
add_dependencies(test_conf sylar)
target_link_libraries(test_conf sylar yaml-cpp::yaml-cpp)

add_executable(test_yaml tests/test_yaml.cc)
# force_redefine_macro_for_sources(test_yaml)
add_dependencies(test_yaml sylar)
target_link_libraries(test_yaml sylar yaml-cpp::yaml-cpp)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)