#include <xc.h>

#if defined(_18FXXQ84_FAMILY_)

#include "../hw.h"
#include "h9def.h"

uint8_t mcu_reset_reason(void) {
    uint8_t reason;

    if (!PCON0bits.nPOR) {
        reason = NODE_RESET_BY_POWER_ON;
    } else if (!PCON0bits.nRWDT) {
        reason = NODE_RESET_BY_WATCHDOG;
    } else if (!PCON0bits.nBOR) {
        reason = NODE_RESET_BY_BROWN_OUT;
    } else if (!PCON0bits.nRI) {
        reason = NODE_RESET_BY_SOFTWARE;
    } else {
        reason = NODE_RESET_BY_UNKNOWN;
    }

    PCON0 = 0x3F;   // reset the flags (nWDTWV, nRWDT, nRMCLR, nRI, nPOR, nBOR), clear STKOVF/STKUNF
    return reason;
}

uint8_t mcu_irq_disable(void) {
    uint8_t state = (uint8_t)(INTCON0bits.GIEH << 1) | INTCON0bits.GIEL;
    INTCON0bits.GIEH = 0;
    INTCON0bits.GIEL = 0;
    return state;
}

void mcu_irq_restore(uint8_t state) {
    INTCON0bits.GIEH = state >> 1;
    INTCON0bits.GIEL = state & 1;
}

#endif /* _18FXXQ84_FAMILY_ */
