cmake_minimum_required(VERSION 3.21)

project(h9can VERSION 2.0)

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "avr")
    include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

    add_subdirectory(avr EXCLUDE_FROM_ALL)
    add_subdirectory(avr_bootloader)
endif ()

add_custom_target(test
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR}/tests
                             -B ${CMAKE_CURRENT_BINARY_DIR}/tests
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/tests
    COMMAND ctest --test-dir ${CMAKE_CURRENT_BINARY_DIR}/tests -V
    COMMENT "Unit tests"
    VERBATIM
)
