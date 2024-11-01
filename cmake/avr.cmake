cmake_minimum_required(VERSION 3.10)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. h9can EXCLUDE_FROM_ALL)

include_directories(${CMAKE_CURRENT_LIST_DIR}/../include)

set(LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/../avr/custom_avr5.lds)

target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:h9can_${MMCU}_${FREQ}>)
#set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT} LINK_FLAGS "-T ${LINKER_SCRIPT}")
