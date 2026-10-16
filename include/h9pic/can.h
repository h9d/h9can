/*
 * h9pic-can v0.2
 *
 * Created by SQ8KFH on 2020-07-11.
 *
 * Copyright (C) 2020 Kamil Palkowski. All rights reserved.
 */

#ifndef CAN_H
#define	CAN_H

#include <xc.h>

extern void (*read_power_supply_register)(uint8_t destination_id, uint8_t seqnum);

#if defined(_18F66K80_FAMILY_)
// Call from the low priority ISR (on PIC18F-Q84 the library has its own vectored CANRX interrupt)
void can_interrupt(void);
#endif

#endif	/* CAN_H */
