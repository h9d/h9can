# ---------------------------------------------------------------------------
# avrdude helpers
# ---------------------------------------------------------------------------
find_program(AVRDUDE avrdude)

if(AVRDUDE_MCU STREQUAL "")
    set(AVRDUDE_MCU ${AVR_MCU})
endif()

if(AVRDUDE)
    set(_AVRDUDE_BASE
            ${AVRDUDE}
            -p ${AVRDUDE_MCU}
            -c ${AVRDUDE_PROGRAMMER}
            -P ${AVRDUDE_PORT}
            -b ${AVRDUDE_BAUD}
            ${AVRDUDE_EXTRA_ARGS}
    )

    if (AVR_MCU STREQUAL atmega64m1)
        set(FUSE -U lfuse:w:0xf4:m -U hfuse:w:0xd5:m -U efuse:w:0xfe:m)
    elseif (AVR_MCU STREQUAL atmega32m1)
        set(FUSE -U lfuse:w:0xf4:m -U hfuse:w:0xd3:m -U efuse:w:0xfe:m)
    elseif (AVR_MCU STREQUAL atmega32c1)
        set(FUSE -U lfuse:w:0xf4:m -U hfuse:w:0xd3:m -U efuse:w:0xfe:m)
    elseif (AVR_MCU STREQUAL atmega16m1)
        set(FUSE -U lfuse:w:0xf4:m -U hfuse:w:0xd3:m -U efuse:w:0xfe:m)
    elseif (AVR_MCU STREQUAL at90can128)
        set(FUSE -U lfuse:w:0xde:m -U hfuse:w:0xdd:m -U efuse:w:0xfd:m)
    endif ()

    # Flash program memory (application only)
    add_custom_target(flash
            COMMAND ${_AVRDUDE_BASE} -U flash:w:${PROJECT_NAME}.hex:i
            DEPENDS ${PROJECT_NAME}
            COMMENT "Flashing ${PROJECT_NAME}.hex to ${AVR_MCU} via ${AVRDUDE_PROGRAMMER} on ${AVRDUDE_PORT}"
    )

    # Write EEPROM
    add_custom_target(flash-eeprom
            COMMAND ${_AVRDUDE_BASE} -U eeprom:w:${PROJECT_NAME}.eep:i
            DEPENDS ${PROJECT_NAME}
            COMMENT "Writing EEPROM to ${AVR_MCU}"
    )

    add_custom_target(fuse
            COMMAND ${_AVRDUDE_BASE} ${FUSE}
            DEPENDS ${PROJECT_NAME}
            COMMENT "Writing fuses to ${AVR_MCU}"
    )

    # Read fuses (non-destructive check)
    add_custom_target(read-fuses
            COMMAND ${_AVRDUDE_BASE}
                    -U lfuse:r:-:h
                    -U hfuse:r:-:h
                    -U efuse:r:-:h
            COMMENT "Reading fuses from ${AVR_MCU}"
    )

    # Verify flash after programming
    add_custom_target(verify
            COMMAND ${_AVRDUDE_BASE} -U flash:v:${PROJECT_NAME}.hex:i
            DEPENDS ${PROJECT_NAME}
            COMMENT "Verifying flash on ${AVR_MCU}"
    )

    # -----------------------------------------------------------------------
    # flash-all  – program bootloader (chip erase) then application (no erase)
    # flash-combined – merge with srec_cat and program in a single pass
    # -----------------------------------------------------------------------
    if(BUILD_BOOTLOADER)
        # Two-pass: erase+write bootloader, then write app without erasing.
        # -D on the second pass preserves the bootloader section.
        add_custom_target(flash-all
                COMMAND ${_AVRDUDE_BASE} -U flash:w:$<TARGET_FILE_DIR:h9can_bootloader_${AVR_MCU}_${FREQ_IN_M}M>/$<TARGET_FILE_BASE_NAME:h9can_bootloader_${AVR_MCU}_${FREQ_IN_M}M>.hex:i
                COMMAND ${_AVRDUDE_BASE} -D -U flash:w:${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex:i
                DEPENDS ${PROJECT_NAME} h9can_bootloader_${AVR_MCU}_${FREQ_IN_M}M
                COMMENT "Programming ${AVR_MCU}: bootloader + application"
        )

        # Single-pass via srec_cat (merge both hex files before flashing)
        find_program(SREC_CAT srec_cat)
        if(SREC_CAT)
            add_custom_target(flash-combined
                    COMMAND ${SREC_CAT}
                            $<TARGET_FILE_DIR:h9can_bootloader_${AVR_MCU}_${FREQ_IN_M}M>/$<TARGET_FILE_BASE_NAME:h9can_bootloader_${AVR_MCU}_${FREQ_IN_M}M>.hex -Intel
                            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex -Intel
                            -o ${CMAKE_BINARY_DIR}/combined.hex -Intel
                    COMMAND ${_AVRDUDE_BASE} -U flash:w:${CMAKE_BINARY_DIR}/combined.hex:i
                    DEPENDS ${PROJECT_NAME} h9can_bootloader_${AVR_MCU}_${FREQ_IN_M}M
                    COMMENT "Single-pass flash of combined bootloader and app image to ${AVR_MCU}"
            )
        endif()
    endif()

else()
    message(WARNING "avrdude not found – flash/verify targets will not be available")
endif()
