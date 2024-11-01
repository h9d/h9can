cmake_minimum_required(VERSION 3.10)
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

function(add_additional_clean_file FILENAME)
    get_directory_property(EXTRA_CLEAN_FILES ADDITIONAL_MAKE_CLEAN_FILES)
    set_directory_properties(
            PROPERTIES
            ADDITIONAL_MAKE_CLEAN_FILES "${EXTRA_CLEAN_FILES};${FILENAME}"
    )

    set_source_files_properties(${FILENAME} PROPERTIES GENERATED TRUE)
endfunction(add_additional_clean_file)


if(NOT BUILD_DIRECTORY)
    set(BUILD_DIRECTORY /build CACHE STRING "Setting build directory to /build")
endif(NOT BUILD_DIRECTORY)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}${BUILD_DIRECTORY}")

if(NOT SOURCE_FILES)
    message(FATAL_ERROR "SOURCE_FILES variable is not defined")
endif(NOT SOURCE_FILES)

#if(NOT FREQ)
#    message(FATAL_ERROR "FREQ variable is not defined")
#endif(NOT FREQ)
#
#if(NOT PROGRAMMER)
#    message(FATAL_ERROR "PROGRAMMER variable is not defined")
#endif(NOT PROGRAMMER)
#
#if(NOT MMCU)
#    message(FATAL_ERROR "MMCU variable is not defined")
#endif(NOT MMCU)
#
#if(NOT BOOTSTART)
#    message(FATAL_ERROR "BOOTSTART variable is not defined")
#endif(NOT BOOTSTART)


configure_file(config.h.in config.h)

#set(PROJECT_NAME avr_bootloader)

message(STATUS "Building ${PROJECT_NAME} for ${MMCU} with frequency ${FREQ} at ${BOOTSTART}")
message(STATUS "Source files: ${SOURCE_FILES}")
message(STATUS "Using flags: ${CMAKE_C_FLAGS}")
message(STATUS "MCU: ${MMCU}")
message(STATUS "F_CPU: ${FREQ}Hz")
message(STATUS "BOOTSTART: ${BOOTSTART}")

#set(ELF_FILENAME ${PROJECT_NAME}.elf)
#
#set(ELF_FILE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${ELF_FILENAME}")
#set(HEX_FILE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}.hex")
#set(EEP_FILE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}.eep")
#
#add_additional_clean_file(${ELF_FILE})
#add_additional_clean_file(${HEX_FILE})
#add_additional_clean_file(${EEP_FILE})

#add_compile_options(
#        -mmcu=${MMCU}
#        -I.
#        -gdwarf-2  //debug options
#        -DF_CPU=${FREQ}UL
#        -Os
#        -funsigned-char
#        -funsigned-bitfields
#        -fpack-struct
#        -fshort-enums
#        -Wall
#        -Wno-unknown-pragmas
#        -Wstrict-prototypes
#        -Wundef
#        -std=gnu99
#        -Wl,--gc-section
#)

if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
    add_compile_options(--param=min-pagesize=0)
    message(STATUS "GCC version >=12 add: --param=min-pagesize=0")
endif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)

#
# ${bootstart_${mmcu}}
#
set(bootstart_atmega16m1 0x3800)
set(bootstart_atmega16c1 0x3800)
set(bootstart_atmega32m1 0x7800)
set(bootstart_atmega32c1 0x7800)
set(bootstart_atmega64m1 0xf800)
set(bootstart_atmega64c1 0xf800)

foreach(mmcu IN ITEMS atmega16m1 atmega32m1 atmega64m1 atmega32c1)
    foreach(freq IN ITEMS 16000000 12000000 4000000)
        set(ELF_FILENAME h9can_bootloader_${mmcu}_${freq}.elf)
        add_executable(h9can_bootloader_${mmcu}_${freq} ${SOURCE_FILES})
        target_compile_options( h9can_bootloader_${mmcu}_${freq} PRIVATE
                        -mmcu=${mmcu}
                        -DF_CPU=${freq}UL
                        -I.
                        -gdwarf-2
                        -Os
                        -funsigned-char
                        -funsigned-bitfields
                        -fpack-struct
                        -fshort-enums
                        -Wall
                        -Wno-unknown-pragmas
                        -Wstrict-prototypes
                        -Wundef
                        -std=gnu99
                        -Wl,--gc-section
                )

        set_target_properties(
                h9can_bootloader_${mmcu}_${freq}
                PROPERTIES
                OUTPUT_NAME ${ELF_FILENAME}
                LINK_FLAGS "-mmcu=${mmcu} -Wl,--section-start=.text=${bootstart_${mmcu}} -I. -gdwarf-2 -DF_CPU=${freq}UL -Os -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -Wall -Wno-unknown-pragmas -Wstrict-prototypes -Wundef -std=gnu99 -Wundef -MD -MP -MF .dep/main.elf.d --output ${ELF_FILENAME} -Wl,-lc,--entry=main"
        )

        add_custom_command(
                TARGET h9can_bootloader_${mmcu}_${freq}
                POST_BUILD
#                COMMAND ${AVR_OBJCOPY} -O ihex -R .eeprom ${ELF_FILENAME} ${HEX_FILE}
                COMMAND ${AVR_OBJDUMP} -P mem-usage ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${ELF_FILENAME}
                COMMENT "Post processsing"
        )
    endforeach()
endforeach()
#add_custom_target("flash_bl"
#        ${AVRDUDE} -p ${MMCU} -c ${PROGRAMMER} -U ${HEX_FILE}
#        DEPENDS ${PROJECT_NAME}
#        COMMENT "Uploading bootloader"
#)
