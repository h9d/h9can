# Runs at build time — generates version.h from git info.
# Inputs (passed via -D):
#   VERSION_MAJOR_BASE   — fallback version major, e.g. "1"
#   VERSION_MINOR_BASE   — fallback version minor, e.g. "0"
#   VERSION_PATCH_BASE   — fallback version patch, e.g. "0"
#   SOURCE_DIR           — repo root
#   TEMPLATE_FILE        — path to version.h.in
#   OUTPUT_FILE          — path where version.h is written

find_package(Git QUIET)

set(GIT_VERSION "")

if(GIT_FOUND)
    # Try "git describe" — works when there is at least one tag reachable
    execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --dirty=-dirty --always
            WORKING_DIRECTORY "${SOURCE_DIR}"
            OUTPUT_VARIABLE GIT_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE GIT_RESULT
    )
    if(GIT_RESULT)
        set(GIT_VERSION "")
    endif()
endif()

set(VERSION_MAJOR ${VERSION_MAJOR_BASE})
set(VERSION_MINOR ${VERSION_MINOR_BASE})
set(VERSION_PATCH ${VERSION_PATCH_BASE})

if(NOT GIT_VERSION)
    set(GIT_VERSION "unknown")
else()
    # If describe returned only a commit hash (no tag reachable), prepend the base version
    string(REGEX MATCH "^v[0-9]" _has_prefix "${GIT_VERSION}")
    if(_has_prefix)
        string(REGEX REPLACE "^v" "" GIT ${GIT_VERSION})
        string(REPLACE "-" ";" GIT_LIST ${GIT})
        list(GET GIT_LIST 0 VERSION_CORE)
        string(REPLACE "." ";" VER_PARTS ${VERSION_CORE})
        list(LENGTH VER_PARTS VER_LEN)

        list(GET VER_PARTS 0 VERSION_MAJOR)
        if(VER_LEN GREATER 1)
            list(GET VER_PARTS 1 VERSION_MINOR)
        else()
            set(VERSION_MINOR 0)
        endif()

        if(VER_LEN GREATER 2)
            list(GET VER_PARTS 2 VERSION_PATCH)
        else()
            set(VERSION_PATCH 0)
        endif()
    endif()
endif()

configure_file("${TEMPLATE_FILE}" "${OUTPUT_FILE}" @ONLY)
message(STATUS "Version: ${GIT_VERSION}")
