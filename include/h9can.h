// SPDX-License-Identifier: MIT
/*
 * H9 CAN protocol
 *
 * Copyright (C) 2024 Kamil Pałkowski
 *
 */

#ifndef H9CAN_H
#define H9CAN_H

#if defined (__AVR_ATmega16M1__) || defined (__AVR_ATmega32M1__) || defined (__AVR_ATmega64M1__) || defined (__AVR_AT90CAN128__) || defined (__AVR_ATmega32C1__)
#  include "avr/can.h"
#else
#error Unsupported MCU
#endif

#endif //H9CAN_H
