// SPDX-License-Identifier: MIT
/*
 * H9 bootloader info block
 *
 * The bootloader places this block at the very end of flash, so the application
 * can read the values compiled into the bootloader (node type, PCB/BOM revision,
 * version) and compare them with its own.
 *
 * Copyright (C) 2026 Kamil Pałkowski
 */

#ifndef H9AVR_BL_INFO_H
#define H9AVR_BL_INFO_H

#include <stdint.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#define H9_BL_INFO_MAGIC  0x4839        // 'H' '9'

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint16_t node_type;
    uint8_t  pcb_rev;                   // ASCII letter ('A', 'B', ...)
    uint8_t  bom_rev;
    uint32_t version;                   // major (10 bits) | minor (11 bits) | patch (11 bits)
} h9_bl_info_t;

// must match H9_BL_INFO_SIZE in avr_bootloader/CMakeLists.txt (.blinfo section address)
#define H9_BL_INFO_SIZE   10
_Static_assert(sizeof(h9_bl_info_t) == H9_BL_INFO_SIZE, "h9_bl_info_t size mismatch");

// last bytes of flash, computed for the current MCU
#define H9_BL_INFO_ADDR   (FLASHEND + 1UL - H9_BL_INFO_SIZE)

/**
 * @brief Read the bootloader info block.
 * @retval 1  Block read and valid.
 * @retval 0  No bootloader, bootloader without the block, or boot section read locked (BLB1x).
 */
static inline uint8_t read_bl_info(h9_bl_info_t *info) {
#if FLASHEND > 0xFFFF       // AT90CAN128: address above 64 KB
    memcpy_PF(info, H9_BL_INFO_ADDR, sizeof(*info));
#else
    memcpy_P(info, (const void *)(uint16_t)H9_BL_INFO_ADDR, sizeof(*info));
#endif
    return info->magic == H9_BL_INFO_MAGIC;
}

#endif /* H9AVR_BL_INFO_H */
