# ---------------------------------------------------------------------------
# avrdude helpers — flash, eeprom, fuse, combined targets
# ---------------------------------------------------------------------------
find_program(AVRDUDE  avrdude)
find_program(SREC_CAT srec_cat)

if (AVRDUDE_MCU STREQUAL "")
    set(AVRDUDE_MCU ${AVR_MCU})
endif()

if (AVRDUDE)
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

    set(_APP_HEX $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.hex)
    set(_APP_EEP $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.eep)

    # Flash program memory (application only)
    add_custom_target(flash
            COMMAND ${_AVRDUDE_BASE} -U flash:w:${_APP_HEX}:i
            DEPENDS ${PROJECT_NAME}
            COMMENT "Flashing ${PROJECT_NAME}.hex to ${AVR_MCU} via ${AVRDUDE_PROGRAMMER} on ${AVRDUDE_PORT}"
    )

    # Write EEPROM
    add_custom_target(flash-eeprom
            COMMAND ${_AVRDUDE_BASE} -U eeprom:w:${_APP_EEP}:i
            DEPENDS ${PROJECT_NAME}
            COMMENT "Writing EEPROM to ${AVR_MCU}"
    )

    add_custom_target(fuse
            COMMAND ${_AVRDUDE_BASE} ${FUSE}
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
            COMMAND ${_AVRDUDE_BASE} -U flash:v:${_APP_HEX}:i
            DEPENDS ${PROJECT_NAME}
            COMMENT "Verifying flash on ${AVR_MCU}"
    )

    # -----------------------------------------------------------------------
    # flash-all  – erase + write bootloader, then write app without erasing.
    # flash-combined – merge with srec_cat and program in a single pass.
    # Both targets depend on FREQ_IN_M being set by avr.cmake before this file
    # is included.
    # -----------------------------------------------------------------------
    set(_BL_TARGET h9can_bootloader_${AVR_MCU}_${FREQ_IN_M}M)
    set(_BL_HEX $<TARGET_FILE_DIR:${_BL_TARGET}>/$<TARGET_FILE_BASE_NAME:${_BL_TARGET}>.hex)

    # Two-pass: erase+write bootloader, then write app without erasing
    add_custom_target(flash-all
            COMMAND ${_AVRDUDE_BASE} -U flash:w:${_BL_HEX}:i
            COMMAND ${_AVRDUDE_BASE} -D -U flash:w:${_APP_HEX}:i
            DEPENDS ${PROJECT_NAME} ${_BL_TARGET}
            COMMENT "Programming ${AVR_MCU}: bootloader + application (two-pass)"
    )

    if (SREC_CAT)
        set(_COMBINED_HEX $<TARGET_FILE_DIR:${PROJECT_NAME}>/combined.hex)
        add_custom_target(flash-combined
                COMMAND ${SREC_CAT}
                        ${_BL_HEX}  -Intel
                        ${_APP_HEX} -Intel
                        -o ${_COMBINED_HEX} -Intel
                COMMAND ${_AVRDUDE_BASE} -U flash:w:${_COMBINED_HEX}:i
                DEPENDS ${PROJECT_NAME} ${_BL_TARGET}
                COMMENT "Single-pass flash of combined bootloader+app image to ${AVR_MCU}"
        )
    else()
        message(STATUS "srec_cat not found – flash-combined target not available")
    endif()

else()
    message(WARNING "avrdude not found – flash/verify targets will not be available")
endif()
