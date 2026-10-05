# h9avr-can
AVR bootloader for a h9 project.

## Example
```
cmake -D FREQ=16000000 -D PROGRAMMER=jtag3isp -D MMCU=atmega64m1 -D BOOTSTART=0xf800 .
make
make flash_bl
```
