cmake_minimum_required(VERSION 3.21)

# When built as a top-level project (standalone), set up the AVR toolchain
# before project() so the compiler is tested against the right target.
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/avr-gcc-toolchain.cmake)
    project(h9can_bootloader LANGUAGES C)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/avr_alt_setting.cmake)

# ---------------------------------------------------------------------------
# Flash-target variant selection
# (when used as a subproject these inherit the parent's cached values)
# ---------------------------------------------------------------------------
set(AVR_MCU   "atmega32m1" CACHE STRING "Bootloader MCU variant to flash")
set(AVR_F_CPU "16000000"   CACHE STRING "CPU frequency in Hz for flash target")

# ---------------------------------------------------------------------------
# Node identity reported in BOOTLOADER_TURNED_ON — required, set with -D
# ---------------------------------------------------------------------------
set(NODE_TYPE    "" CACHE STRING "Node type reported by the bootloader (e.g. 0x0102)")
set(PCB_REVISION "" CACHE STRING "PCB revision reported by the bootloader (letter A-Z)")
set(BOM_REVISION "" CACHE STRING "BOM revision reported by the bootloader (0-255)")

set(_MISSING_NODE_VARS "")
foreach (_var IN ITEMS NODE_TYPE PCB_REVISION BOM_REVISION)
    if ("${${_var}}" STREQUAL "")
        list(APPEND _MISSING_NODE_VARS ${_var})
    endif ()
endforeach ()

if (NOT "${PCB_REVISION}" STREQUAL "" AND NOT PCB_REVISION MATCHES "^[A-Z]$")
    message(FATAL_ERROR "PCB_REVISION must be a single letter A-Z, got '${PCB_REVISION}'")
endif ()

# ---------------------------------------------------------------------------
# Versioning — git describe from h9can repo root
# ---------------------------------------------------------------------------
set(_H9CAN_ROOT    ${CMAKE_CURRENT_LIST_DIR}/..)
set(_CONFIG_H_IN   ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in)
set(_CONFIG_H_OUT  ${CMAKE_CURRENT_BINARY_DIR}/config.h)
set(_GIT_DESCRIBE  ${_H9CAN_ROOT}/cmake/git_describe.cmake)

set(_VERSION_ARGS
    -D VERSION_MAJOR_BASE=2
    -D VERSION_MINOR_BASE=0
    -D VERSION_PATCH_BASE=0
    -D SOURCE_DIR=${_H9CAN_ROOT}
    -D TEMPLATE_FILE=${_CONFIG_H_IN}
    -D OUTPUT_FILE=${_CONFIG_H_OUT}
    -D NODE_TYPE=${NODE_TYPE}
    -D PCB_REVISION=${PCB_REVISION}
    -D BOM_REVISION=${BOM_REVISION}
    -P "${_GIT_DESCRIBE}"
)

if (_MISSING_NODE_VARS)
    # Don't fail configuration (h9can is also pulled in by application projects),
    # fail only when the bootloader itself is being built.
    string(REPLACE ";" ", " _MISSING_NODE_VARS "${_MISSING_NODE_VARS}")
    message(STATUS "Bootloader: ${_MISSING_NODE_VARS} not set (-D<VAR>=<value>), bootloader targets will not build")

    add_custom_target(generate_bootloader_version ALL
        COMMAND ${CMAKE_COMMAND} -E echo "ERROR: bootloader requires ${_MISSING_NODE_VARS}, set with -D<VAR>=<value> (see avr_bootloader/README.md)"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
else ()
    # Generate an initial config.h at configure time
    execute_process(COMMAND ${CMAKE_COMMAND} ${_VERSION_ARGS})

    # Re-generate on every build so the version string stays current
    add_custom_target(generate_bootloader_version ALL
        COMMAND ${CMAKE_COMMAND} ${_VERSION_ARGS}
        COMMENT "Generating bootloader config.h"
    )
endif ()

# ---------------------------------------------------------------------------
# Build all MCU × frequency variants
# ---------------------------------------------------------------------------
set(SOURCE_FILES bootloader.c can.c can.h ../include/h9def.h ../include/h9frame.h)

# bootloader info block (h9avr/bl_info.h) at the end of flash: FLASHEND + 1 - H9_BL_INFO_SIZE
set(H9_BL_INFO_SIZE 10)

foreach (mmcu IN LISTS avr_mmcus)
    math(EXPR _BLINFO_ADDR "${flashend_${mmcu}} + 1 - ${H9_BL_INFO_SIZE}" OUTPUT_FORMAT HEXADECIMAL)
    foreach (freq IN LISTS avr_freqs)
        set(TARGET h9can_bootloader_${mmcu}_${freq})

        add_executable(${TARGET} ${SOURCE_FILES})
        set_target_properties(${TARGET} PROPERTIES SUFFIX ".elf")
        target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../include")
        target_compile_options(${TARGET} PRIVATE
                -mmcu=${mmcu}
                -DF_CPU=${fcpu_${freq}}
                -DBOOTSTART=${bootstart_${mmcu}}
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
                )
        target_link_options(${TARGET} PRIVATE
                -mmcu=${mmcu}
                -Wl,--entry=main,--section-start=.text=${bootstart_${mmcu}},-lc,--gc-section,-Map=$<TARGET_FILE_DIR:${TARGET}>/$<TARGET_FILE_BASE_NAME:${TARGET}>.map
                -Wl,--section-start=.blinfo=${_BLINFO_ADDR},--undefined=bl_info     # keep the block, place it at the end of flash
                -Wall
                )

        add_custom_command(
                TARGET ${TARGET}
                POST_BUILD
                COMMAND ${AVR_OBJCOPY} -O ihex -R .eeprom $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/$<TARGET_FILE_BASE_NAME:${TARGET}>.hex
                COMMAND ${AVR_OBJDUMP} -P mem-usage $<TARGET_FILE:${TARGET}>
        )

        add_dependencies(${TARGET} generate_bootloader_version)
    endforeach ()
endforeach ()

# ---------------------------------------------------------------------------
# Optional flash target for a selected variant (AVR_MCU × AVR_F_CPU)
# ---------------------------------------------------------------------------
find_program(AVRDUDE avrdude)
if (AVRDUDE)
    set(AVRDUDE_PROGRAMMER "jtag3isp" CACHE STRING "avrdude programmer (-c)")
    set(AVRDUDE_PORT       "usb"      CACHE STRING "avrdude port (-P)")
    set(AVRDUDE_BAUD       "115200"   CACHE STRING "avrdude baud rate (-b)")
    set(AVRDUDE_EXTRA_ARGS ""         CACHE STRING "Extra avrdude arguments")

    math(EXPR _FREQ_M "${AVR_F_CPU} / 1000000")
    set(_FLASH_TARGET h9can_bootloader_${AVR_MCU}_${_FREQ_M}M)

    add_custom_target(flash-bootloader
            COMMAND ${AVRDUDE}
                    -p ${AVR_MCU}
                    -c ${AVRDUDE_PROGRAMMER}
                    -P ${AVRDUDE_PORT}
                    -b ${AVRDUDE_BAUD}
                    ${AVRDUDE_EXTRA_ARGS}
                    -U flash:w:$<TARGET_FILE_DIR:${_FLASH_TARGET}>/$<TARGET_FILE_BASE_NAME:${_FLASH_TARGET}>.hex:i
            DEPENDS ${_FLASH_TARGET}
            COMMENT "Flashing ${_FLASH_TARGET} to ${AVR_MCU} via ${AVRDUDE_PROGRAMMER} on ${AVRDUDE_PORT}"
    )
else()
    message(STATUS "avrdude not found – flash-bootloader target not available")
endif()
