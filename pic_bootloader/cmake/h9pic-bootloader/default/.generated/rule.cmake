# The following functions contains all the flags passed to the different build stages.

set(PACK_REPO_PATH "/Users/crowx/.mchp_packs" CACHE PATH "Path to the root of a pack repository.")

function(h9pic_bootloader_default_default_XC8_assemble_rule target)
    set(options
        "-c"
        "${MP_EXTRA_AS_PRE}"
        "-mcpu=18F46K80"
        "${DEBUGGER_NAME}"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC18F-K_DFP/1.16.308/xc8"
        "-fno-short-double"
        "-fno-short-float"
        "-memi=wordwrite"
        "-mrom=f600-ffff"
        "-O2"
        "-fasmfile"
        "-maddrqual=ignore"
        "-mwarn=-3"
        "-Wa,-a"
        "-msummary=-psect,-class,+mem,-hex,-file"
        "-mcodeoffset=f600"
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
        PRIVATE "__18F46K80__"
        PRIVATE "__DEBUG=1"
        PRIVATE "__MPLAB_DEBUGGER_PK4=1"
        PRIVATE "XPRJ_default=default")
    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
endfunction()
function(h9pic_bootloader_default_default_XC8_assemblePreprocess_rule target)
    set(options
        "-c"
        "${MP_EXTRA_AS_PRE}"
        "-mcpu=18F46K80"
        "-x"
        "assembler-with-cpp"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC18F-K_DFP/1.16.308/xc8"
        "-fno-short-double"
        "-fno-short-float"
        "-memi=wordwrite"
        "-mrom=f600-ffff"
        "-O2"
        "-fasmfile"
        "-maddrqual=ignore"
        "-mwarn=-3"
        "-Wa,-a"
        "-msummary=-psect,-class,+mem,-hex,-file"
        "-mcodeoffset=f600"
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
        PRIVATE "__18F46K80__"
        PRIVATE "__DEBUG=1"
        PRIVATE "__MPLAB_DEBUGGER_PK4=1"
        PRIVATE "XPRJ_default=default")
    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
endfunction()
function(h9pic_bootloader_default_default_XC8_compile_rule target)
    set(options
        "-c"
        "${MP_EXTRA_CC_PRE}"
        "-mcpu=18F46K80"
        "${DEBUGGER_NAME}"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC18F-K_DFP/1.16.308/xc8"
        "-fno-short-double"
        "-fno-short-float"
        "-memi=wordwrite"
        "-mrom=f600-ffff"
        "-O2"
        "-fasmfile"
        "-maddrqual=ignore"
        "-mwarn=-3"
        "-Wa,-a"
        "-msummary=-psect,-class,+mem,-hex,-file"
        "-mcodeoffset=f600"
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
        PRIVATE "__18F46K80__"
        PRIVATE "__DEBUG=1"
        PRIVATE "__MPLAB_DEBUGGER_PK4=1"
        PRIVATE "XPRJ_default=default")
    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
endfunction()
function(h9pic_bootloader_default_link_rule target)
    set(options
        "-Wl,-Map=mem.map"
        "${MP_EXTRA_LD_PRE}"
        "-mcpu=18F46K80"
        "${DEBUGGER_NAME}"
        "-Wl,--defsym=__MPLAB_BUILD=1"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC18F-K_DFP/1.16.308/xc8"
        "-fno-short-double"
        "-fno-short-float"
        "-memi=wordwrite"
        "-mrom=f600-ffff"
        "-O2"
        "-fasmfile"
        "-maddrqual=ignore"
        "-mwarn=-3"
        "-Wa,-a"
        "-msummary=-psect,-class,+mem,-hex,-file"
        "-mcodeoffset=f600"
        "-ginhx32"
        "-Wl,--data-init"
        "-mno-keep-startup"
        "-mno-download"
        "-mdefault-config-bits"
        "-std=c99"
        "-gdwarf-3"
        "-mstack=compiled:auto:auto:auto"
        "-mrom=default"
        "-mram=default,ffd-fff"
        "-Wl,--memorysummary,memoryfile.xml")
    list(REMOVE_ITEM options "")
    target_link_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target}
        PRIVATE "__DEBUG=1"
        PRIVATE "XPRJ_default=default")
    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
endfunction()
