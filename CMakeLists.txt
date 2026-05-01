cmake_minimum_required(VERSION 3.10)
project(10kgp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(EMIT_SRC emit/emit.c)
set(WIN_EXTRA_SRC "")
if(WIN32)
  set(EMIT_SRC emit/win_emit.c)
  list(APPEND WIN_EXTRA_SRC win_stdin.c)
endif()

add_executable(10kgp
    main.c main.h
    header.h
    win_header.h
    ${EMIT_SRC}
    emit/emit.h
    emit/emit_bcast.c
    ${WIN_EXTRA_SRC}
)

target_include_directories(10kgp PRIVATE ${CMAKE_SOURCE_DIR})

if(WIN32)
  target_link_libraries(10kgp PRIVATE ws2_32 iphlpapi)
endif()

add_library(persi STATIC
    persi/persi_value.c
    persi/persi_format.c
    persi/persi_core.c
    persi/persi_sql.c
)
target_include_directories(persi PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(persi_demo persi/demo.c)
target_link_libraries(persi_demo PRIVATE persi)
