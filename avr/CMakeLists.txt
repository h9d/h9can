include(${CMAKE_CURRENT_LIST_DIR}/../cmake/avr_alt_setting.cmake)

foreach (mmcu IN LISTS avr_mmcus)
    foreach (freq IN LISTS avr_freqs)
        add_library(h9can_${mmcu}_${freq} OBJECT can.c)
        target_compile_options(h9can_${mmcu}_${freq} PRIVATE
                -mmcu=${mmcu}
                -DF_CPU=${fcpu_${freq}}
                -DBOOTSTART=${bootstart_${mmcu}}
                -Os
                -gdwarf-2
                -funsigned-char
                -funsigned-bitfields
                -fpack-struct
                -fshort-enums
                -Wall
                -Wno-unknown-pragmas
                -Wstrict-prototypes
                -Wundef
                -std=gnu11
                )
    endforeach ()
endforeach ()
