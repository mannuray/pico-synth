cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_BOARD pico CACHE STRING "Board type")

if(NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "PICO_SDK_PATH is not set. Export it before configuring this project.")
endif()

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(synth_pico C CXX ASM)

pico_sdk_init()

add_executable(synth_pico
    src/main.c
    src/audio_i2s.c
    src/keyboard_matrix.c
    src/mic_adc.c
    src/synth_engine.c
)

pico_generate_pio_header(synth_pico ${CMAKE_CURRENT_LIST_DIR}/src/pio/i2s_out.pio)

target_include_directories(synth_pico PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(synth_pico PRIVATE
    pico_stdlib
    hardware_adc
    hardware_clocks
    hardware_dma
    hardware_irq
    hardware_pio
)

pico_enable_stdio_uart(synth_pico 1)
pico_enable_stdio_usb(synth_pico 0)

pico_add_extra_outputs(synth_pico)
