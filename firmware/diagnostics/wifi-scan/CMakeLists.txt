cmake_minimum_required(VERSION 3.13)

if (NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "PICO_SDK_PATH must point to the Raspberry Pi Pico SDK")
endif()

set(PICO_BOARD pico_w CACHE STRING "Target board")
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(p2wp_wifi_scan_diagnostic C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(p2wp-wifi-scan-diagnostic main.c)
target_compile_options(p2wp-wifi-scan-diagnostic PRIVATE
    -Wall
    -Wextra
    -Werror
    # SDK 2.1.1's CYW43 no-lwIP callback stubs intentionally leave their
    # parameters unused.
    -Wno-error=unused-parameter
)
target_compile_definitions(p2wp-wifi-scan-diagnostic PRIVATE CYW43_LWIP=0)
target_link_libraries(p2wp-wifi-scan-diagnostic PRIVATE
    pico_stdlib
    # Scanning is a CYW43 operation and does not need the IP stack. Keeping
    # lwIP out makes this a genuine radio-only diagnostic.
    pico_cyw43_arch_poll
)

pico_enable_stdio_uart(p2wp-wifi-scan-diagnostic 0)
pico_enable_stdio_usb(p2wp-wifi-scan-diagnostic 1)
pico_add_extra_outputs(p2wp-wifi-scan-diagnostic)
