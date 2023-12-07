cmake_minimum_required(VERSION 3.0)

# Set the project name
project(LoggerTest)

# Set clang++ as the compiler
# set(CMAKE_CXX_COMPILER "clang++")
## or command CXX=clang++ cmake


# Add an executable with the above sources
add_executable(logger_test logger.cpp logger_test.cpp)