cmake_minimum_required(VERSION 3.13)
include(CMakeDependentOption)

find_program(AVR_C_COMPILER avr-gcc)
find_program(AVR_CXX_COMPILER avr-g++)
find_program(AVR_OBJCOPY avr-objcopy)
find_program(AVR_OBJDUMP avr-objdump)
find_program(AVRDUDE avrdude)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)
set(CMAKE_C_COMPILER ${AVR_C_COMPILER})
set(CMAKE_CXX_COMPILER ${AVR_CXX_COMPILER})


set(SOURCE_FILES bootloader.c can.c can.h ../include/h9def.h ../include/h9msg.h)


if (NOT BUILD_DIRECTORY)
    set(BUILD_DIRECTORY /build CACHE STRING "Setting build directory to /build")
endif (NOT BUILD_DIRECTORY)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}${BUILD_DIRECTORY}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

#if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
#    add_compile_options(--param=min-pagesize=0)
#endif (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/avr_alt_setting.cmake)

foreach (mmcu IN LISTS avr_mmcus)
    foreach (freq IN LISTS avr_freqs)
        set(TARGET h9can_bootloader_${mmcu}_${freq})

        add_executable(${TARGET} ${SOURCE_FILES})
        set_target_properties(${TARGET} PROPERTIES SUFFIX ".elf")
        target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
        target_compile_options(${TARGET} PRIVATE
                -mmcu=${mmcu}
                -DF_CPU=${fcpu_${freq}}
                -Os
                -gdwarf-2
                -funsigned-char
                -funsigned-bitfields
                -fpack-struct
                -fshort-enums
                -Wall
                -Wno-unknown-pragmas
                -Wstrict-prototypes
                -Wundef
                -std=gnu11
                )
        target_link_options(${TARGET} PRIVATE
                -mmcu=${mmcu}
                -std=gnu11
                -Wl,--entry=main,--section-start=.text=${bootstart_${mmcu}},-lc,--gc-section,-Map=$<TARGET_FILE_DIR:${TARGET}>/$<TARGET_FILE_BASE_NAME:${TARGET}>.map}
                -Wall
                )

        add_custom_command(
                TARGET ${TARGET}
                POST_BUILD
                COMMAND ${AVR_OBJCOPY} -O ihex -R .eeprom $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/$<TARGET_FILE_BASE_NAME:${TARGET}>.hex
#                COMMAND ${AVR_OBJDUMP} -P mem-usage $<TARGET_FILE:${TARGET}>
        )
    endforeach ()
endforeach ()
