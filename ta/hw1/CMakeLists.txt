cmake_minimum_required(VERSION 3.6)
project(hw1)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES tokenizer.c shell.c test.c)
add_executable(shell ${SOURCE_FILES})