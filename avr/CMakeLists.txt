find_program(AVR_C_COMPILER avr-gcc REQUIRED)
find_program(AVR_CXX_COMPILER avr-g++ REQUIRED)
find_program(AVR_OBJCOPY avr-objcopy REQUIRED)
find_program(AVR_SIZE avr-size REQUIRED)
find_program(AVRDUDE avrdude)
find_program(SREC_CAT srec_cat REQUIRED)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)
set(CMAKE_C_COMPILER ${AVR_C_COMPILER})
set(CMAKE_CXX_COMPILER ${AVR_CXX_COMPILER})

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
