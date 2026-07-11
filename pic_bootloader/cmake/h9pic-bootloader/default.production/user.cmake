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

include(${CMAKE_SOURCE_DIR}/../../../../cmake/git-version.cmake)
