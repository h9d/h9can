# ---------------------------------------------------------------------------
# version.h generation
# ---------------------------------------------------------------------------
set(VERSION_H_IN  "${CMAKE_SOURCE_DIR}/../../../version.h.in")
set(VERSION_H_OUT "${CMAKE_BINARY_DIR}/version.h")

set(VERSION_MAJOR_BASE 0)
set(VERSION_MINOR_BASE 0)
set(VERSION_PATCH_BASE 0)

set(VERSION_DEPENDS_TARGET "h9pic_bootloader_default_default_XC8_compile")

get_filename_component(_version_h_dir "${VERSION_H_OUT}" DIRECTORY)
target_include_directories("h9pic_bootloader_default_default_XC8_compile" PRIVATE "${_version_h_dir}")

# ---------------------------------------------------------------------------
# Node identity reported in BOOTLOADER_TURNED_ON — required, set with -D
# ---------------------------------------------------------------------------
set(NODE_TYPE    "" CACHE STRING "Node type reported by the bootloader (e.g. 0x0102)")
set(PCB_REVISION "" CACHE STRING "PCB revision reported by the bootloader (letter A-Z)")
set(BOM_REVISION "" CACHE STRING "BOM revision reported by the bootloader (0-255)")

foreach (_var IN ITEMS NODE_TYPE PCB_REVISION BOM_REVISION)
    if ("${${_var}}" STREQUAL "")
        message(FATAL_ERROR "${_var} not set, configure with -D${_var}=<value> (see pic_bootloader/README.md)")
    endif ()
endforeach ()
if (NOT PCB_REVISION MATCHES "^[A-Z]$")
    message(FATAL_ERROR "PCB_REVISION must be a single letter A-Z, got '${PCB_REVISION}'")
endif ()

# XC8 drops the quotes of a char literal passed with -D, so pass the ASCII code ('B' -> 0x42)
string(HEX "${PCB_REVISION}" _PCB_REVISION_HEX)

target_compile_definitions("h9pic_bootloader_default_default_XC8_compile" PRIVATE
    NODE_TYPE=${NODE_TYPE}
    PCB_REVISION=0x${_PCB_REVISION_HEX}
    BOM_REVISION=${BOM_REVISION}
)

include(${CMAKE_SOURCE_DIR}/../../../../cmake/git-version.cmake)
