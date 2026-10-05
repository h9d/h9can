#
# ${bootstart_${mmcu}}
#
set(bootstart_atmega16m1 0x3800)
set(bootstart_atmega16c1 0x3800)
set(bootstart_atmega32m1 0x7800)
set(bootstart_atmega32c1 0x7800)
set(bootstart_atmega64m1 0xf800)
set(bootstart_atmega64c1 0xf800)
set(bootstart_at90can128 0x1F800)

#
# ${flashend_${mmcu}} - last flash address (FLASHEND)
#
set(flashend_atmega16m1 0x3FFF)
set(flashend_atmega16c1 0x3FFF)
set(flashend_atmega32m1 0x7FFF)
set(flashend_atmega32c1 0x7FFF)
set(flashend_atmega64m1 0xFFFF)
set(flashend_atmega64c1 0xFFFF)
set(flashend_at90can128 0x1FFFF)

set(fcpu_4M 4000000UL)
set(fcpu_12M 12000000UL)
set(fcpu_16M 16000000UL)

set(avr_mmcus atmega16m1 atmega32m1 atmega64m1 atmega32c1 at90can128)
set(avr_freqs 4M 12M 16M)
