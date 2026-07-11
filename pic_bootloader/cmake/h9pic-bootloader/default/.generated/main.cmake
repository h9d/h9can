include("${CMAKE_CURRENT_LIST_DIR}/rule.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/file.cmake")

set(h9pic_bootloader_default_library_list )

# Handle files with suffix (s|as|asm|AS|ASM|As|aS|Asm), for group default-XC8
if(h9pic_bootloader_default_default_XC8_FILE_TYPE_assemble)
add_library(h9pic_bootloader_default_default_XC8_assemble OBJECT ${h9pic_bootloader_default_default_XC8_FILE_TYPE_assemble})
    h9pic_bootloader_default_default_XC8_assemble_rule(h9pic_bootloader_default_default_XC8_assemble)
    list(APPEND h9pic_bootloader_default_library_list "$<TARGET_OBJECTS:h9pic_bootloader_default_default_XC8_assemble>")

endif()

# Handle files with suffix S, for group default-XC8
if(h9pic_bootloader_default_default_XC8_FILE_TYPE_assemblePreprocess)
add_library(h9pic_bootloader_default_default_XC8_assemblePreprocess OBJECT ${h9pic_bootloader_default_default_XC8_FILE_TYPE_assemblePreprocess})
    h9pic_bootloader_default_default_XC8_assemblePreprocess_rule(h9pic_bootloader_default_default_XC8_assemblePreprocess)
    list(APPEND h9pic_bootloader_default_library_list "$<TARGET_OBJECTS:h9pic_bootloader_default_default_XC8_assemblePreprocess>")

endif()

# Handle files with suffix [cC], for group default-XC8
if(h9pic_bootloader_default_default_XC8_FILE_TYPE_compile)
add_library(h9pic_bootloader_default_default_XC8_compile OBJECT ${h9pic_bootloader_default_default_XC8_FILE_TYPE_compile})
    h9pic_bootloader_default_default_XC8_compile_rule(h9pic_bootloader_default_default_XC8_compile)
    list(APPEND h9pic_bootloader_default_library_list "$<TARGET_OBJECTS:h9pic_bootloader_default_default_XC8_compile>")

endif()


# Main target for this project
add_executable(h9pic_bootloader_default_image_pqJsbh6X ${h9pic_bootloader_default_library_list})

set_target_properties(h9pic_bootloader_default_image_pqJsbh6X PROPERTIES
    OUTPUT_NAME "default"
    SUFFIX ".elf"
    ADDITIONAL_CLEAN_FILES "${output_extensions}"
    RUNTIME_OUTPUT_DIRECTORY "${h9pic_bootloader_default_output_dir}")
target_link_libraries(h9pic_bootloader_default_image_pqJsbh6X PRIVATE ${h9pic_bootloader_default_default_XC8_FILE_TYPE_link})

# Add the link options from the rule file.
h9pic_bootloader_default_link_rule( h9pic_bootloader_default_image_pqJsbh6X)


