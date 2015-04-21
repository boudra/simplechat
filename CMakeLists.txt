cmake_minimum_required(VERSION 2.6)
project(simplechat)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

file(GLOB server_files "src/server/*.cpp")

include_directories(${simplechat_SOURCE_DIR}/src)

add_executable(server ${server_files})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -g -pedantic -std=c++11 -pthread")
