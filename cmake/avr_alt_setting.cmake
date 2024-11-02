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

set(fcpu_4M 4000000UL)
set(fcpu_12M 12000000UL)
set(fcpu_16M 16000000UL)

set(avr_mmcus atmega16m1 atmega32m1 atmega64m1 atmega32c1 at90can128)
set(avr_freqs 4M 12M 16M)
