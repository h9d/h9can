#include <xc.h>

#if defined(_18FXXQ84_FAMILY_)

#include <stdint.h>

// Data EEPROM at 0x380000 in the NVM address space
void eeprom_write_byte(uint16_t addr, uint8_t data) {
    uint8_t GIEBitValue = INTCON0bits.GIE;
    NVMADRU = 0x38;
    NVMADRH = ((addr >> 8) & 0x03);
    NVMADRL = (addr & 0xFF);
    NVMDATL = data;
    NVMCON1bits.CMD = 0x03;     // write byte
    INTCON0bits.GIE = 0;        // Disable interrupts
    NVMLOCK = 0x55;
    NVMLOCK = 0xAA;
    NVMCON0bits.GO = 1;
    // Wait for write to complete
    while (NVMCON0bits.GO);
    INTCON0bits.GIE = GIEBitValue;  // restore interrupt enable
    NVMCON1bits.CMD = 0x00;
}

uint8_t eeprom_read_byte(uint16_t addr) {
    NVMADRU = 0x38;
    NVMADRH = ((addr >> 8) & 0x03);
    NVMADRL = (addr & 0xFF);
    NVMCON1bits.CMD = 0x00;     // read byte
    NVMCON0bits.GO = 1;
    while (NVMCON0bits.GO);

    return NVMDATL;
}

#endif /* _18FXXQ84_FAMILY_ */
