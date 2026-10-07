// SPDX-License-Identifier: MIT
/*
 * H9 CAN bootloader for AVR
 *
 * Copyright (C) 2018-2024 Kamil Pałkowski
 *
 */

#include "config.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>

#include "../include/h9def.h"
#include "../include/h9frame.h"
#include "can.h"
#include "../include/h9avr/bl_info.h"

// version packed into 32 bits: major (10 bits) | minor (11 bits) | patch (11 bits)
#if BOOTLOADER_VERSION_MAJOR > 0x3ff || BOOTLOADER_VERSION_MINOR > 0x7ff || BOOTLOADER_VERSION_PATCH > 0x7ff
#error "Bootloader version out of range (major 0-1023, minor/patch 0-2047)"
#endif
#define BOOTLOADER_VERSION_PACKED (((uint32_t)BOOTLOADER_VERSION_MAJOR << 22) | ((uint32_t)BOOTLOADER_VERSION_MINOR << 11) | (uint32_t)BOOTLOADER_VERSION_PATCH)

// info block at the end of flash, read by the application (see h9avr/bl_info.h)
const h9_bl_info_t bl_info __attribute__((section(".blinfo"), used)) = {
    .magic     = H9_BL_INFO_MAGIC,
    .node_type = NODE_TYPE,
    .pcb_rev   = PCB_REVISION,
    .bom_rev   = BOM_REVISION,
    .version   = BOOTLOADER_VERSION_PACKED,
};

#if FLASHEND > 0xffff
typedef uint32_t flash_addr_t;
#else
typedef uint16_t flash_addr_t;
#endif

// pages below the bootloader section, BOOTSTART is set by CMake
#define APP_PAGES (BOOTSTART / SPM_PAGESIZE)

// host id is locked at the first PAGE_START, frames from other nodes are ignored since then
#define HOST_ID_NONE 0x100

// receive timeouts (about 1-2 s each) in a row before the page is abandoned
#define PAGE_FILL_RETRIES 4

static uint8_t seqnum = 0;

static void write_page(uint16_t page, uint8_t host_id) {
    uint16_t bytes_remain = SPM_PAGESIZE;
    flash_addr_t addr = (flash_addr_t)page * SPM_PAGESIZE;

    h9frame_t cm_res;
    cm_res.source_id = can_node_id;
    cm_res.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;
    cm_res.unicast.destination_id = host_id;

    boot_page_erase_safe(addr);
    while (1) {
        // every new PAGE_FILL_NEXT gets the next seqnum, the host copies it to PAGE_FILL
        cm_res.type = H9FRAME_TYPE_PAGE_FILL_NEXT;
        cm_res.unicast.seqnum = ++seqnum;
        cm_res.dlc = 2;
        cm_res.data[0] = (bytes_remain >> 8) & 0xff;
        cm_res.data[1] = (bytes_remain) & 0xff;
        CAN_put_msg_blocking(&cm_res);

        h9frame_t cm;
        uint8_t retries = PAGE_FILL_RETRIES;
        while (1) {
            if (!CAN_get_msg_blocking(&cm)) {
                // PAGE_FILL or the response was lost - repeat the response (the same seqnum)
                if (--retries == 0)
                    goto page_break;
                CAN_put_msg_blocking(&cm_res);
                continue;
            }
            if (cm.source_id != host_id)
                continue;
            // any other bootloader frame from the host aborts the page
            if (cm.type != H9FRAME_TYPE_PAGE_FILL)
                goto page_break;
            // PAGE_FILL answering an earlier (repeated) response is a duplicate, ignored
            if (cm.dlc == 8 && cm.unicast.seqnum == cm_res.unicast.seqnum)
                break;
        }

        for (uint8_t i = 0; i < 8; i += 2) {
            boot_page_fill_safe(addr + (SPM_PAGESIZE - bytes_remain), cm.data[i + 1] << 8 | cm.data[i]);
            bytes_remain -= 2;
        }

        if (bytes_remain == 0) {
            boot_page_write_safe(addr);
            boot_spm_busy_wait();
            boot_rww_enable();

            cm_res.type = H9FRAME_TYPE_PAGE_WRITED;
            cm_res.data[0] = (page >> 8) & 0xff;
            cm_res.data[1] = (page) & 0xff;
            CAN_put_msg_blocking(&cm_res);
            return;
        }
    }

page_break:
    cm_res.type = H9FRAME_TYPE_PAGE_FILL_BREAK;
    cm_res.dlc = 0;
    CAN_put_msg_blocking(&cm_res);
}

int main(void) {
    DDRB = 0xff;
    DDRC = 0xff;
    DDRD = 0xff;
    DDRE = 0xff;
    cli();
    MCUCR |= (1<<IVCE);
    MCUCR |= (1<<IVSEL);
    cli();

    CAN_init();
    
    h9frame_t turn_on_msg;

    turn_on_msg.type = H9FRAME_TYPE_BOOTLOADER_TURNED_ON;
    turn_on_msg.source_id = can_node_id;
    turn_on_msg.broadcast.group = NODE_TYPE;
    turn_on_msg.dlc = 8;

    // version big-endian
    const uint32_t version = BOOTLOADER_VERSION_PACKED;
    turn_on_msg.data[0] = (version >> 24) & 0xff;
    turn_on_msg.data[1] = (version >> 16) & 0xff;
    turn_on_msg.data[2] = (version >> 8) & 0xff;
    turn_on_msg.data[3] = version & 0xff;

    turn_on_msg.data[4] = PCB_REVISION;
    turn_on_msg.data[5] = BOM_REVISION;

#if defined (__AVR_ATmega16M1__)
    turn_on_msg.data[6] = NODE_MCU_ATMEGA16M1;
#elif defined (__AVR_ATmega32M1__)
    turn_on_msg.data[6] = NODE_MCU_ATMEGA32M1;
#elif defined (__AVR_ATmega64M1__)
    turn_on_msg.data[6] = NODE_MCU_ATMEGA64M1;
#elif defined (__AVR_AT90CAN128__)
    turn_on_msg.data[6] = NODE_MCU_AT90CAN128;
#elif defined (__AVR_ATmega32C1__)
    turn_on_msg.data[6] = NODE_MCU_ATMEGA32C1;
#else
#error "Unsupported MCU"
#endif

#if F_CPU == 4000000UL
    turn_on_msg.data[7] = NODE_MCU_F_4MHz;
#elif F_CPU == 12000000UL
    turn_on_msg.data[7] = NODE_MCU_F_12MHz;
#elif F_CPU == 16000000UL
    turn_on_msg.data[7] = NODE_MCU_F_16MHz;
#else
#error "Please specify F_CPU"
#endif

    CAN_put_msg_blocking(&turn_on_msg);
    
    uint16_t host_id = HOST_ID_NONE;
    while (1) {
        h9frame_t cm;
        if (CAN_get_msg_blocking(&cm)) {
            if (host_id != HOST_ID_NONE && cm.source_id != host_id)
                continue;

            if (cm.type == H9FRAME_TYPE_PAGE_START && cm.dlc == 2) {
                uint16_t page = cm.data[0] << 8 | cm.data[1];
                if (page < APP_PAGES) {
                    host_id = cm.source_id;
                    write_page(page, host_id);
                }
            }
            else if (cm.type == H9FRAME_TYPE_QUIT_BOOTLOADER && cm.dlc == 0) {
                MCUCR &= ~(1 << IVSEL);
                __asm__ volatile ("jmp  0x0000");
            }
        }
        else {
            CAN_put_msg_blocking(&turn_on_msg);
        }
    }
}

