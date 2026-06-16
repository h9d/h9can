cmake_minimum_required(VERSION 3.13)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. h9can EXCLUDE_FROM_ALL)

include_directories(${CMAKE_CURRENT_LIST_DIR}/../include)

set(LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/../avr/custom_avr5.lds)

#if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
#    add_compile_options(--param=min-pagesize=0)
#    message(STATUS "GCC version >=12 add: --param=min-pagesize=0")
#endif (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${CMAKE_CURRENT_BINARY_DIR}/build_information_gen.h)
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

math(EXPR FREQ_IN_M ${FREQ}/1000000)
target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:h9can_${MMCU}_${FREQ_IN_M}M>)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/build_information_gen.h
        COMMENT "Generate build information"
        COMMAND sh ${CMAKE_CURRENT_LIST_DIR}/../tools/build_information.sh > ${CMAKE_CURRENT_BINARY_DIR}/build_information_gen.h
        DEPENDS $<TARGET_OBJECTS:h9can_${MMCU}_${FREQ_IN_M}M> ${SOURCE_FILES}
)
add_custom_target(build_information DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/build_information_gen.h h9can_${MMCU}_${FREQ_IN_M}M)
add_dependencies(${PROJECT_NAME} build_information)

add_dependencies(${PROJECT_NAME} h9can_bootloader_${MMCU}_${FREQ_IN_M}M)

add_custom_target(${PROJECT_NAME}_with_bl
        COMMAND ${SREC_CAT} ${HEX_FILE} -I $<TARGET_FILE_DIR:h9can_bootloader_${MMCU}_${FREQ_IN_M}M>/$<TARGET_FILE_BASE_NAME:h9can_bootloader_${MMCU}_${FREQ_IN_M}M>.hex -I -o ${HEX_WITH_BL_FILE} -I
        DEPENDS ${PROJECT_NAME} h9can_bootloader_${MMCU}_${FREQ_IN_M}M
        COMMENT "Flashs combinating"
)
