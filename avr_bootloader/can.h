// SPDX-License-Identifier: MIT
/*
 * H9 CAN bootloader for AVR
 *
 * Copyright (C) 2020-2024 Kamil Pałkowski
 *
 */

#ifndef _CAN_H_
#define _CAN_H_

#include <avr/io.h>
#include <avr/eeprom.h>

#include "../include/h9frame.h"

extern uint8_t can_node_id;
extern uint16_t can_node_type;

void CAN_init(void);

void CAN_put_msg_blocking(h9frame_t *cm);
uint8_t CAN_get_msg_blocking(h9frame_t *cm);

#endif //_CAN_H_
