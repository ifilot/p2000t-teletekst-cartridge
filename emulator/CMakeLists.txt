cmake_minimum_required(VERSION 3.16)
project(p2000t_teletekst_emulator C)
set(CMAKE_C_STANDARD 11)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(CURL REQUIRED libcurl)
add_executable(p2000t-emulator src/main.c src/p2wp_device.c src/page_backend.c
  vendor/m2000/P2000.c vendor/m2000/Z80.c ../firmware/src/p2wp.c ../firmware/src/teletekst.c)
target_include_directories(p2000t-emulator PRIVATE vendor/m2000 src ../firmware/src ${SDL2_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})
target_compile_options(p2000t-emulator PRIVATE -Wall -Wextra -Werror ${SDL2_CFLAGS_OTHER} ${CURL_CFLAGS_OTHER})
set_source_files_properties(vendor/m2000/Z80.c vendor/m2000/P2000.c
  PROPERTIES COMPILE_OPTIONS "-Wno-implicit-fallthrough")
target_link_libraries(p2000t-emulator PRIVATE ${SDL2_LIBRARIES} ${CURL_LIBRARIES})
