cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

include(pico_extras_import_optional.cmake)

project(bktovga C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()
pico_sdk_init()
add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        -DDBG=1
        )



add_executable(bktovga
        main.c
        vga.c
        capture.c
        )

pico_generate_pio_header(bktovga ${CMAKE_CURRENT_LIST_DIR}/vga.pio)
pico_generate_pio_header(bktovga ${CMAKE_CURRENT_LIST_DIR}/capture.pio)
target_include_directories(bktovga PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# pull in common dependencies
target_link_libraries(bktovga  pico_stdlib  hardware_pio  hardware_dma pico_multicore pico_time)
#enable usb output, disable uart output4
#    pico_enable_stdio_usb(bktovga 1)
#    pico_enable_stdio_uart(main 1)
# create map/bin/hex file etc.
    pico_add_extra_outputs(bktovga)

# add url via pico_set_program_url
#example_auto_set_url(...)

