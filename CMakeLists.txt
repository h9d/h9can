cmake_minimum_required(VERSION 3.10)

project(h9can VERSION 1.3)

include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

add_subdirectory(avr EXCLUDE_FROM_ALL)
add_subdirectory(avr_bootloader)
