# The following functions contains all the flags passed to the different build stages.

set(PACK_REPO_PATH "/Users/crowx/.mchp_packs" CACHE PATH "Path to the root of a pack repository.")

function(h9pic_bootloader_q84_q84_XC8_assemble_rule target)
    set(options
        "-c"
        "${MP_EXTRA_AS_PRE}"
        "-mcpu=18F47Q84"
        "${DEBUGGER_NAME}"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC18F-Q_DFP/1.28.451/xc8"
        "-fno-short-double"
        "-fno-short-float"
        "-memi=wordwrite"
        "-mrom=1e000-1ffff"
        "-O2"
        "-fasmfile"
        "-maddrqual=ignore"
        "-mwarn=-3"
        "-Wa,-a"
        "-msummary=-psect,-class,+mem,-hex,-file"
        "-mcodeoffset=1e000"
        "-ginhx32"
        "-Wl,--data-init"
        "-mno-keep-startup"
        "-mno-download"
        "-mdefault-config-bits"
        "-std=c99"
        "-gdwarf-3"
        "-mstack=compiled:auto:auto:auto")
    list(REMOVE_ITEM options "")
    target_compile_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target}
        PRIVATE "__18F47Q84__"
        PRIVATE "__DEBUG=1"
        PRIVATE "__MPLAB_DEBUGGER_PK4=1"
        PRIVATE "XPRJ_q84=q84")
    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
endfunction()
function(h9pic_bootloader_q84_q84_XC8_assemblePreprocess_rule target)
    set(options
        "-c"
        "${MP_EXTRA_AS_PRE}"
        "-mcpu=18F47Q84"
        "-x"
        "assembler-with-cpp"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC18F-Q_DFP/1.28.451/xc8"
        "-fno-short-double"
        "-fno-short-float"
        "-memi=wordwrite"
        "-mrom=1e000-1ffff"
        "-O2"
        "-fasmfile"
        "-maddrqual=ignore"
        "-mwarn=-3"
        "-Wa,-a"
        "-msummary=-psect,-class,+mem,-hex,-file"
        "-mcodeoffset=1e000"
        "-ginhx32"
        "-Wl,--data-init"
        "-mno-keep-startup"
        "-mno-download"
        "-mdefault-config-bits"
        "-std=c99"
        "-gdwarf-3"
        "-mstack=compiled:auto:auto:auto")
    list(REMOVE_ITEM options "")
    target_compile_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target}
        PRIVATE "__18F47Q84__"
        PRIVATE "__DEBUG=1"
        PRIVATE "__MPLAB_DEBUGGER_PK4=1"
        PRIVATE "XPRJ_q84=q84")
    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
endfunction()
function(h9pic_bootloader_q84_q84_XC8_compile_rule target)
    set(options
        "-c"
        "${MP_EXTRA_CC_PRE}"
        "-mcpu=18F47Q84"
        "${DEBUGGER_NAME}"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC18F-Q_DFP/1.28.451/xc8"
        "-fno-short-double"
        "-fno-short-float"
        "-memi=wordwrite"
        "-mrom=1e000-1ffff"
        "-O2"
        "-fasmfile"
        "-maddrqual=ignore"
        "-mwarn=-3"
        "-Wa,-a"
        "-msummary=-psect,-class,+mem,-hex,-file"
        "-mcodeoffset=1e000"
        "-ginhx32"
        "-Wl,--data-init"
        "-mno-keep-startup"
        "-mno-download"
        "-mdefault-config-bits"
        "-std=c99"
        "-gdwarf-3"
        "-mstack=compiled:auto:auto:auto")
    list(REMOVE_ITEM options "")
    target_compile_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target}
        PRIVATE "__18F47Q84__"
        PRIVATE "__DEBUG=1"
        PRIVATE "__MPLAB_DEBUGGER_PK4=1"
        PRIVATE "XPRJ_q84=q84")
    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
endfunction()
function(h9pic_bootloader_q84_link_rule target)
    set(options
        "-Wl,-Map=mem.map"
        "${MP_EXTRA_LD_PRE}"
        "-mcpu=18F47Q84"
        "${DEBUGGER_NAME}"
        "-Wl,--defsym=__MPLAB_BUILD=1"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC18F-Q_DFP/1.28.451/xc8"
        "-fno-short-double"
        "-fno-short-float"
        "-memi=wordwrite"
        "-mrom=1e000-1ffff"
        "-O2"
        "-fasmfile"
        "-maddrqual=ignore"
        "-mwarn=-3"
        "-Wa,-a"
        "-msummary=-psect,-class,+mem,-hex,-file"
        "-mcodeoffset=1e000"
        "-ginhx32"
        "-Wl,--data-init"
        "-mno-keep-startup"
        "-mno-download"
        "-mdefault-config-bits"
        "-std=c99"
        "-gdwarf-3"
        "-mstack=compiled:auto:auto:auto"
        "-mrom=default"
        "-mram=default"
        "-Wl,--memorysummary,memoryfile.xml")
    list(REMOVE_ITEM options "")
    target_link_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target}
        PRIVATE "__DEBUG=1"
        PRIVATE "XPRJ_q84=q84")
    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
endfunction()
function(h9pic_bootloader_q84_objcopy_lss_rule target)
    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${OBJDUMP}
        ARGS --disassemble --wide --demangle --line-numbers --section-headers --source ${h9pic_bootloader_q84_image_name} > ${h9pic_bootloader_q84_image_base_name}.lss
        WORKING_DIRECTORY ${h9pic_bootloader_q84_output_dir})
endfunction()
