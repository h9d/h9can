/*
 * h9pic-bootloader
 *
 * Created by SQ8KFH on 2020-07-29.
 *
 * Copyright (C) 2020 Kamil Palkowski. All rights reserved.
 */

#ifndef CAN_H
#define	CAN_H

#include <xc.h>

#include "h9msg.h"

extern volatile uint16_t can_node_id;

void CAN_init(void);

void CAN_put_msg_blocking(h9msg_t *cm);
uint8_t CAN_get_msg_blocking(h9msg_t*cm);

#endif	/* CAN_H */
