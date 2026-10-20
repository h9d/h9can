#include <xc.h>

#if defined(_18F66K80_FAMILY_)

#include "../hw.h"
#include "h9def.h"

uint8_t mcu_reset_reason(void) {
    uint8_t reason;

    if (!RCONbits.POR && !RCONbits.BOR) {
        reason = NODE_RESET_BY_POWER_ON;    // POR=0, BOR=0 → power-on
    } else if (RCONbits.TO == 0) {
        reason = NODE_RESET_BY_WATCHDOG;    // TO=0 → WDT timeout
    } else if (!RCONbits.BOR) {
        reason = NODE_RESET_BY_BROWN_OUT;   // BOR=0 → brown-out
    } else if (!RCONbits.RI) {
        reason = NODE_RESET_BY_SOFTWARE;    // RI=0 → RESET instr.
    //} else if (!RCONbits.RMCLR) {
    //    reason = NODE_RESET_BY_EXTERNAL_SOURCE; // MCLR pin
    } else {
        reason = NODE_RESET_BY_UNKNOWN;
    }

    //reset przyczyny resetu:D
    RCONbits.POR = 1;
    RCONbits.BOR = 1;
    RCONbits.RI  = 1;
    return reason;
}

uint8_t mcu_irq_disable(void) {
    uint8_t state = (uint8_t)(INTCONbits.GIEH << 1) | INTCONbits.GIEL;
    INTCONbits.GIEH = 0;
    INTCONbits.GIEL = 0;
    return state;
}

void mcu_irq_restore(uint8_t state) {
    INTCONbits.GIEH = state >> 1;
    INTCONbits.GIEL = state & 1;
}

#endif /* _18F66K80_FAMILY_ */
