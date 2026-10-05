// SPDX-License-Identifier: MIT
/*
 * H9 bootloader info block (PIC)
 *
 * The bootloader places this block at the very end of flash, so the application
 * can read the values compiled into the bootloader (node type, PCB/BOM revision,
 * version) and compare them with its own. Same layout as h9avr/bl_info.h.
 *
 * Copyright (C) 2026 Kamil Pałkowski
 */

#ifndef H9PIC_BL_INFO_H
#define H9PIC_BL_INFO_H

#include <stdint.h>
#include <xc.h>

#define H9_BL_INFO_MAGIC  0x4839        // 'H' '9'

typedef struct {
    uint16_t magic;
    uint16_t node_type;
    uint8_t  pcb_rev;                   // ASCII letter ('A', 'B', ...)
    uint8_t  bom_rev;
    uint32_t version;                   // major (10 bits) | minor (11 bits) | patch (11 bits)
} h9_bl_info_t;

#define H9_BL_INFO_SIZE   10
_Static_assert(sizeof(h9_bl_info_t) == H9_BL_INFO_SIZE, "h9_bl_info_t size mismatch");

// last bytes of flash (PIC18F46K80: 0xFFF6), inside the bootloader area (0xF600-0xFFFF)
#define H9_BL_INFO_ADDR   ((uint24_t)_ROMSIZE - H9_BL_INFO_SIZE)

/**
 * @brief Read the bootloader info block.
 * @retval 1  Block read and valid.
 * @retval 0  No bootloader, bootloader without the block, or bootloader area table-read protected (EBTRx).
 */
static inline uint8_t read_bl_info(h9_bl_info_t *info) {
    uint8_t *p = (uint8_t *)info;

    TBLPTRU = (uint8_t)(H9_BL_INFO_ADDR >> 16);
    TBLPTRH = (uint8_t)(H9_BL_INFO_ADDR >> 8);
    TBLPTRL = (uint8_t)H9_BL_INFO_ADDR;
    for (uint8_t i = 0; i < H9_BL_INFO_SIZE; ++i) {
        asm("TBLRD*+");
        p[i] = TABLAT;
    }
    return info->magic == H9_BL_INFO_MAGIC;
}

#endif /* H9PIC_BL_INFO_H */
