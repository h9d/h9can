/*
 * MCU specific definitions shared by the PIC library and the PIC bootloader.
 *
 * Supported: PIC18F46K80 (ECAN) and PIC18F47Q84 (CAN FD module, used in CAN 2.0 mode).
 */

#ifndef H9PIC_MCU_H
#define H9PIC_MCU_H

#include <xc.h>
#include "h9def.h"

#if defined(_18FXXQ84_FAMILY_)
#define H9PIC_Q84 1

#if !defined(_18F47Q84)
#error "Only PIC18F47Q84 from the Q84 family is supported (NODE_MCU, bootloader address)"
#endif

#define H9PIC_NODE_MCU          NODE_MCU_PIC18F47Q84
#define H9PIC_BOOTLOADER_ADDR   0x1E000     // pic_bootloader code offset, last 8 KB of flash
#define H9PIC_INTCON            INTCON0bits

#else

#define H9PIC_NODE_MCU          NODE_MCU_PIC18F46K80
#define H9PIC_BOOTLOADER_ADDR   0xF600      // pic_bootloader code offset
#define H9PIC_INTCON            INTCONbits

#endif

#endif /* H9PIC_MCU_H */
