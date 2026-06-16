# Generates a version header at configure time and on every build via a
# custom target.
#
# Required variables (set before including):
#   VERSION_H_IN          — path to the .in template
#   VERSION_H_OUT         — path where the generated header is written
#
# Optional variables:
#   GIT_SOURCE_DIR        — repo root for "git describe" (default: CMAKE_SOURCE_DIR)
#   VERSION_MAJOR_BASE    — fallback major  (default: PROJECT_VERSION_MAJOR)
#   VERSION_MINOR_BASE    — fallback minor  (default: PROJECT_VERSION_MINOR)
#   VERSION_PATCH_BASE    — fallback patch  (default: PROJECT_VERSION_PATCH)
#   VERSION_TARGET_NAME   — custom-target name (default: generate_version)
#   VERSION_DEPENDS_TARGET — target that depends on versioning (default: PROJECT_NAME)

if (NOT DEFINED GIT_SOURCE_DIR)
    set(GIT_SOURCE_DIR ${CMAKE_SOURCE_DIR})
endif()
if (NOT DEFINED VERSION_MAJOR_BASE)
    set(VERSION_MAJOR_BASE ${PROJECT_VERSION_MAJOR})
endif()
if (NOT DEFINED VERSION_MINOR_BASE)
    set(VERSION_MINOR_BASE ${PROJECT_VERSION_MINOR})
endif()
if (NOT DEFINED VERSION_PATCH_BASE)
    set(VERSION_PATCH_BASE ${PROJECT_VERSION_PATCH})
endif()
if (NOT DEFINED VERSION_TARGET_NAME)
    set(VERSION_TARGET_NAME generate_version)
endif()
if (NOT DEFINED VERSION_DEPENDS_TARGET)
    set(VERSION_DEPENDS_TARGET ${PROJECT_NAME})
endif()

set(_GIT_DESCRIBE_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/git_describe.cmake)

set(_GIT_DESCRIBE_ARGS
    -D VERSION_MAJOR_BASE=${VERSION_MAJOR_BASE}
    -D VERSION_MINOR_BASE=${VERSION_MINOR_BASE}
    -D VERSION_PATCH_BASE=${VERSION_PATCH_BASE}
    -D SOURCE_DIR=${GIT_SOURCE_DIR}
    -D TEMPLATE_FILE=${VERSION_H_IN}
    -D OUTPUT_FILE=${VERSION_H_OUT}
    -P "${_GIT_DESCRIBE_SCRIPT}"
)

# Custom target re-runs the script on every build
add_custom_target(${VERSION_TARGET_NAME} ALL
    COMMAND ${CMAKE_COMMAND} ${_GIT_DESCRIBE_ARGS}
    COMMENT "Generating ${VERSION_H_OUT}"
)

# Generate an initial header at configure time so the first build never starts without it
execute_process(COMMAND ${CMAKE_COMMAND} ${_GIT_DESCRIBE_ARGS})

add_dependencies(${VERSION_DEPENDS_TARGET} ${VERSION_TARGET_NAME})
