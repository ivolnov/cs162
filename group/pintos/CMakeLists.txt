cmake_minimum_required(VERSION 3.6)
project(pintos)

set(CMAKE_C_STANDARD 99)

include_directories(
        src
        src/lib/kernel
        src/threads
        src/devices
        src/utils
        src/lib
)

file(GLOB SOURCE_FILES
        *.c
        *.h
        */*.c
        */*.h
        */*/*.S
        */*/*.c
        */*/*.h
        */*/*/*.c
        */*/*/*.h
        )

add_executable(pintos ${SOURCE_FILES})