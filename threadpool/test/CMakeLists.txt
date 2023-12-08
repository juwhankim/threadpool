cmake_minimum_required(VERSION 3.0)

# Set the project name
project(LoggerTest
        LANGUAGES CXX
        VERSION 0.1.0)

# Set clang++ as the compiler
set(CMAKE_CXX_COMPILER "clang++")
## or command CXX=clang++ cmake

message(STATUS "Compiler")
message(STATUS " - ID       \t: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS " - Version  \t: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS " - Path     \t: ${CMAKE_CXX_COMPILER}")

set(CMAKE_BUILD_TYPE Debug)

message(STATUS CMAKE_BUILD_TYPE)                        # -- CMAKE_BUILD_TYPE
message(STATUS ${CMAKE_BUILD_TYPE})                     # -- Debug
message(STATUS "Configuration: ${CMAKE_BUILD_TYPE}")    # -- Configuration: Debug

#clang -std=c++11
#greedy_threadpool_test.cpp
#greedy_threadpool.cpp
# -o greedy_threadpool_test -I../logger -lpthread -lstdc++ -O3
include_directories(
        ../
)
# Add an executable with the above sources
add_executable(
        threadpool_test
        threadpool_test.cpp
)

#add_library(
#        logger_test
#        logger.cpp
#        logger_test.cpp)