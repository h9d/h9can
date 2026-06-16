cmake_minimum_required(VERSION 3.21)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src/")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../include")

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. h9can EXCLUDE_FROM_ALL)

set(LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/../avr/custom_avr5.lds)

# ---------------------------------------------------------------------------
# Compiler / linker flags
# ---------------------------------------------------------------------------
target_compile_options(${PROJECT_NAME} PRIVATE
        -mmcu=${AVR_MCU}
        -DF_CPU=${AVR_F_CPU}UL
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

target_link_options(${PROJECT_NAME} PRIVATE
        -mmcu=${AVR_MCU}
        -Wl,--entry=main,-lc,--gc-section,-Map=$<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.map
        -Wall
)

# ---------------------------------------------------------------------------
# Post-build: .hex, .bin, size report
# ---------------------------------------------------------------------------
add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${AVR_OBJCOPY} -O ihex   -R .eeprom $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.hex
        COMMAND ${AVR_OBJCOPY} -O binary -R .eeprom $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.bin
        COMMAND ${AVR_OBJCOPY} -O ihex -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 --no-change-warnings $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.eep
        COMMAND ${AVR_OBJDUMP} -P mem-usage $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Building ${PROJECT_NAME}.hex / .bin / .eep"
)

# ---------------------------------------------------------------------------
# Add h9can dependencies
# ---------------------------------------------------------------------------
math(EXPR FREQ_IN_M ${AVR_F_CPU}/1000000)
target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:h9can_${AVR_MCU}_${FREQ_IN_M}M>)


# ---------------------------------------------------------------------------
# Add bootloader dependencies
# ---------------------------------------------------------------------------
add_dependencies(${PROJECT_NAME} h9can_bootloader_${AVR_MCU}_${FREQ_IN_M}M)


# ---------------------------------------------------------------------------
# Disassembly listing
# ---------------------------------------------------------------------------
add_custom_target(disasm
        COMMAND ${AVR_OBJDUMP} -d -S $<TARGET_FILE:${PROJECT_NAME}> > ${PROJECT_NAME}.lst
        DEPENDS ${PROJECT_NAME}.elf
        COMMENT "Disassembling → ${PROJECT_NAME}.lst"
)
