cmake_minimum_required(VERSION 3.10)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. h9can EXCLUDE_FROM_ALL)

include_directories(${CMAKE_CURRENT_LIST_DIR}/../include)

set(LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/../avr/custom_avr5.lds)

math(EXPR FREQ_IN_M ${FREQ}/1000000)
target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:h9can_${MMCU}_${FREQ_IN_M}M>)
