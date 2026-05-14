cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()

if(NOT PICO_SDK_PATH)
    message(FATAL_ERROR "Set PICO_SDK_PATH to your pico-sdk checkout before configuring.")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(musickeyboard_rp2350_smoke C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(musickeyboard_rp2350_smoke
    main.c
    ../src/platform/rp2350/st7789_rp2350.c
)

target_include_directories(musickeyboard_rp2350_smoke PRIVATE
    ../include
)

target_link_libraries(musickeyboard_rp2350_smoke
    pico_stdlib
    hardware_spi
    hardware_pwm
)

pico_enable_stdio_uart(musickeyboard_rp2350_smoke 0)
pico_enable_stdio_usb(musickeyboard_rp2350_smoke 1)

pico_add_extra_outputs(musickeyboard_rp2350_smoke)
