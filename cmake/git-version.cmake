# Custom target that re-runs the cmake script on every build
add_custom_target(generate_version ALL
        COMMAND ${CMAKE_COMMAND}
        -D VERSION_MAJOR_BASE=${PROJECT_VERSION_MAJOR}
        -D VERSION_MINOR_BASE=${PROJECT_VERSION_MINOR}
        -D VERSION_PATCH_BASE=${PROJECT_VERSION_PATCH}
        -D SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -D TEMPLATE_FILE=${VERSION_H_IN}
        -D OUTPUT_FILE=${VERSION_H_OUT}
        -P "${H9CAN_PATH}/cmake/git_describe.cmake"
        COMMENT "Generating version.h"
)

# Generate an initial version.h at configure time so the build never starts without it
execute_process(
        COMMAND ${CMAKE_COMMAND}
        -D VERSION_MAJOR_BASE=${PROJECT_VERSION_MAJOR}
        -D VERSION_MINOR_BASE=${PROJECT_VERSION_MINOR}
        -D VERSION_PATCH_BASE=${PROJECT_VERSION_PATCH}
        -D SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -D TEMPLATE_FILE=${VERSION_H_IN}
        -D OUTPUT_FILE=${VERSION_H_OUT}
        -P "${H9CAN_PATH}/cmake/git_describe.cmake"
)

add_dependencies(${PROJECT_NAME} generate_version)
