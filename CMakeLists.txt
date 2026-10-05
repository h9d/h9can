cmake_minimum_required(VERSION 3.13)

project(h9can VERSION 1.3)

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "avr")
    include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

    add_subdirectory(avr EXCLUDE_FROM_ALL)
    add_subdirectory(avr_bootloader)
endif ()

#if (${CMAKE_PROJECT_NAME} STREQUAL "h9can")
#    include_directories(${CMAKE_CURRENT_LIST_DIR}/include)
#
#    if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "avr")
#
#    add_subdirectory(avr EXCLUDE_FROM_ALL)
#    add_subdirectory(avr_bootloader)
#endif ()
