cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()

if(NOT PICO_SDK_PATH)
    message(FATAL_ERROR "Set PICO_SDK_PATH to your pico-sdk checkout before configuring.")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(musickeyboard_rp2350_bringup C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(MK_FATFS_SOURCE_DIR ${PICO_SDK_PATH}/lib/tinyusb/lib/fatfs/source)

pico_sdk_init()

add_executable(musickeyboard_rp2350_bringup
    main.c
    ../src/app.c
    ../src/core/transport.c
    ../src/core/sample_bank.c
    ../src/core/sequencer.c
    ../src/core/audio_engine.c
    ../src/platform/rp2350/audio_hal_rp2350.c
    ../src/platform/rp2350/diskio_sd_spi_rp2350.c
    ../src/platform/rp2350/d200_link_rp2350.c
    ../src/platform/rp2350/storage_hal_rp2350.c
    ${MK_FATFS_SOURCE_DIR}/ff.c
    ${MK_FATFS_SOURCE_DIR}/ffunicode.c
)

target_include_directories(musickeyboard_rp2350_bringup PRIVATE
    ../include
    ${MK_FATFS_SOURCE_DIR}
)

target_link_libraries(musickeyboard_rp2350_bringup
    pico_stdlib
    hardware_pwm
    hardware_spi
    hardware_timer
    hardware_sync
)

pico_enable_stdio_uart(musickeyboard_rp2350_bringup 0)
pico_enable_stdio_usb(musickeyboard_rp2350_bringup 1)

pico_add_extra_outputs(musickeyboard_rp2350_bringup)
