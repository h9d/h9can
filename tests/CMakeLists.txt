cmake_minimum_required(VERSION 3.21)
project(h9can_tests C)

set(CMAKE_C_STANDARD 11)

set(STUB_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/stubs)
set(H9CAN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Unity — use system installation, fall back to FetchContent
find_path(UNITY_INCLUDE_DIR unity.h PATH_SUFFIXES unity)
find_library(UNITY_LIB unity)

if(NOT UNITY_INCLUDE_DIR OR NOT UNITY_LIB)
    message(STATUS "Unity not found on system, fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(
        unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG        v2.6.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(unity)
    set(UNITY_INCLUDE_DIR ${unity_SOURCE_DIR}/src)
    set(UNITY_LIB unity)
else()
    message(STATUS "Using system Unity: ${UNITY_LIB}")
endif()

add_executable(test_process_msg
    test_process_msg.c
    ${H9CAN_DIR}/avr/can.c
)

# Stub dir must be first so our avr/*.h and errno.h shadow system headers
target_include_directories(test_process_msg BEFORE PRIVATE
    ${STUB_DIR}
    ${H9CAN_DIR}/include
    ${UNITY_INCLUDE_DIR}
)

target_compile_definitions(test_process_msg PRIVATE
    F_CPU=16000000UL
    __AVR_ATmega32M1__=1
)

target_link_libraries(test_process_msg ${UNITY_LIB})

enable_testing()
add_test(NAME process_msg COMMAND test_process_msg)
