cmake_minimum_required(VERSION 3.13)

if (NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "PICO_SDK_PATH must point to the Raspberry Pi Pico SDK")
endif()

set(PICO_BOARD pico_w CACHE STRING "Target board")
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" P2WP_RELEASE_VERSION LIMIT_COUNT 1)
project(p2wp_pico VERSION "${P2WP_RELEASE_VERSION}" LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(p2wp-pico
    src/main.c
    src/custom_endpoint.c
    src/custom_url_store.c
    src/firmware_core.c
    src/p2wp.c
    src/teletekst.c
    src/version.c
    src/wifi_profile.c
)

# Embed release metadata in the UF2 so `picotool info` can identify exactly
# which firmware is installed without relying on the output filename.
pico_set_program_name(p2wp-pico "P2000T Teletekst Pico")
pico_set_program_version(p2wp-pico "${PROJECT_VERSION}")
pico_set_program_description(
    p2wp-pico
    "P2WP/2-5 Wi-Fi and Teletekst firmware"
)

target_include_directories(p2wp-pico PRIVATE src)
set_source_files_properties(
    src/main.c
    src/custom_endpoint.c
    src/custom_url_store.c
    src/firmware_core.c
    src/p2wp.c
    src/teletekst.c
    src/version.c
    src/wifi_profile.c
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror"
)
target_compile_definitions(p2wp-pico PRIVATE
    PICO_CORE1_STACK_SIZE=4096
    ALTCP_MBEDTLS_AUTHMODE=MBEDTLS_SSL_VERIFY_REQUIRED
)
target_link_libraries(p2wp-pico PRIVATE
    pico_stdlib
    pico_multicore
    hardware_gpio
    hardware_timer
    hardware_flash
    pico_cyw43_arch_lwip_poll
    pico_flash
    pico_lwip_http
    pico_lwip_mbedtls
    pico_mbedtls
    pico_rand
    pico_unique_id
)

# GPIO0 and GPIO1 are part of the parallel input bus, so UART stdio cannot be
# enabled. USB stdio is also omitted to keep the link test independent of USB.
pico_enable_stdio_uart(p2wp-pico 0)
pico_enable_stdio_usb(p2wp-pico 0)
pico_add_extra_outputs(p2wp-pico)
