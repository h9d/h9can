#include <xc.h>

#if defined(_18F66K80_FAMILY_)

#include <stdint.h>

void eeprom_write_byte(uint16_t addr, uint8_t data) {
    uint8_t GIEBitValue = INTCONbits.GIE;
    EEADRH = ((addr >> 8) & 0x03);
    EEADR = (addr & 0xFF);
    EEDATA = data;
    EECON1bits.EEPGD = 0;
    EECON1bits.CFGS = 0;
    EECON1bits.WREN = 1;
    INTCONbits.GIE = 0;     // Disable interrupts
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1;
    // Wait for write to complete
    while (EECON1bits.WR);
    EECON1bits.WREN = 0;
    INTCONbits.GIE = GIEBitValue;   // restore interrupt enable
}

uint8_t eeprom_read_byte(uint16_t addr) {
    EEADRH = ((addr >> 8) & 0x03);
    EEADR = (addr & 0xFF);
    EECON1bits.CFGS = 0;
    EECON1bits.EEPGD = 0;
    EECON1bits.RD = 1;
    NOP();  // NOPs may be required for latency at high frequencies
    NOP();

    return (EEDATA);
}

#endif /* _18F66K80_FAMILY_ */
