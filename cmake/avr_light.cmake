cmake_minimum_required(VERSION 3.13)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. h9can EXCLUDE_FROM_ALL)

include_directories(${CMAKE_CURRENT_LIST_DIR}/../include)

set(LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/../avr/custom_avr5.lds)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(${PROJECT_NAME} PRIVATE
        -mmcu=${MMCU}
        -DF_CPU=${FREQ}UL
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

target_link_options(${PROJECT_NAME} PRIVATE
        -mmcu=${MMCU}
        -std=gnu11
        -Wl,--entry=main,-lc,--gc-section,-Map=$<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.map
        -Wall
)

add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${AVR_OBJCOPY} -O ihex -R .eeprom $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.hex
        COMMAND ${AVR_OBJCOPY} -O ihex -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.eep
        COMMAND ${AVR_OBJDUMP} -P mem-usage $<TARGET_FILE:${PROJECT_NAME}>
)
