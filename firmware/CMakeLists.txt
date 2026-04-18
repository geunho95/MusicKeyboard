cmake_minimum_required(VERSION 3.20)

project(musickeyboard_sampler C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(musickeyboard_sampler
    src/main.c
    src/app.c
    src/core/transport.c
    src/core/sample_bank.c
    src/core/sequencer.c
    src/core/audio_engine.c
    src/platform/host/audio_hal_host.c
    src/platform/host/d200_link_host.c
    src/platform/host/storage_hal_host.c
)

target_include_directories(musickeyboard_sampler PRIVATE include)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(musickeyboard_sampler PRIVATE -Wall -Wextra -Wpedantic)
endif()
