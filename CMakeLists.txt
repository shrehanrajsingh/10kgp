cmake_minimum_required(VERSION 3.10)
project(10kgp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(10kgp
    main.c main.h
    header.h
    emit/emit.c emit/emit.h
)

target_include_directories(10kgp PRIVATE ${CMAKE_SOURCE_DIR})
