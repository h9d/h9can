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

static uint8_t seqnum = 0;

void write_page(uint16_t page, uint16_t dst_id) {
    uint16_t bytes_remain = SPM_PAGESIZE;
    page = page * SPM_PAGESIZE;

    boot_page_erase_safe(page);
    while (1) {
        h9frame_t cm;
        CAN_get_msg_blocking(&cm);

        h9frame_t cm_res;

        cm_res.source_id = can_node_id;
        cm_res.unicast.flags = H9FRAME_FLAG_SINGE_MSG;
        cm_res.unicast.destination_id = dst_id;
        cm_res.unicast.seqnum = cm.unicast.seqnum;

        if (cm.source_id == dst_id && cm.type == H9FRAME_TYPE_PAGE_FILL && cm.dlc == 8) {
            uint16_t w = cm.data[1] << 8;
            w |= cm.data[0];

            boot_page_fill_safe(page + (SPM_PAGESIZE-bytes_remain), w);
            bytes_remain -= 2;

            w = cm.data[3] << 8;
            w |= cm.data[2];

            boot_page_fill_safe(page + (SPM_PAGESIZE-bytes_remain), w);
            bytes_remain -= 2;

            w = cm.data[5] << 8;
            w |= cm.data[4];

            boot_page_fill_safe(page + (SPM_PAGESIZE-bytes_remain), w);
            bytes_remain -= 2;

            w = cm.data[7] << 8;
            w |= cm.data[6];

            boot_page_fill_safe(page + (SPM_PAGESIZE-bytes_remain), w);
            bytes_remain -= 2;

            if (bytes_remain == 0) {
                cm_res.type = H9FRAME_TYPE_PAGE_WRITED;
                cm_res.dlc = 2;
                cm_res.data[0] = (page >> 8) & 0xff;
                cm_res.data[1] = (page) & 0xff;

                boot_page_write_safe(page);
                boot_spm_busy_wait();
                boot_rww_enable();

                CAN_put_msg_blocking(&cm_res);
                break;
            }
            else {
                cm_res.type = H9FRAME_TYPE_PAGE_FILL_NEXT;
                cm_res.dlc = 2;
                cm_res.data[0] = (bytes_remain >> 8) & 0xff;
                cm_res.data[1] = (bytes_remain) & 0xff;
                CAN_put_msg_blocking(&cm_res);
            }
        }
        else if (cm.source_id == dst_id && (cm.type & H9FRAME_BOOTLOADER_MSG_TYPE_GROUP_MASK) == H9FRAME_BOOTLOADER_MSG_TYPE_GROUP) {
            cm_res.type = H9FRAME_TYPE_PAGE_FILL_BREAK;
            cm_res.dlc = 0;

            CAN_put_msg_blocking(&cm_res);
            break;
        }
    }
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
    PORTC = (PORTC & 0x0C) | (0xaa & 0xF3);
    PORTD = (PORTD & 0xFC) | ((0xaa>>2) & 0x03);

    CAN_init();
    
    h9frame_t turn_on_msg;

    turn_on_msg.type = H9FRAME_TYPE_BOOTLOADER_TURNED_ON;
    turn_on_msg.source_id = can_node_id;
    turn_on_msg.broadcast.group = can_node_type;
    turn_on_msg.dlc = 8;

	turn_on_msg.data[0] = (can_node_type >> 8) & 0xff;
    turn_on_msg.data[1] = (can_node_type) & 0xff;

    turn_on_msg.data[2] = (BOOTLOADER_VERSION_MAJOR >> 8) & 0xff;
    turn_on_msg.data[3] = (BOOTLOADER_VERSION_MAJOR) & 0xff;
    turn_on_msg.data[4] = (BOOTLOADER_VERSION_MINOR >> 8);
    turn_on_msg.data[5] = BOOTLOADER_VERSION_MINOR & 0xff;
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
    
    while (1) {
        h9frame_t cm;
        if (CAN_get_msg_blocking(&cm)) {
            if (cm.type == H9FRAME_TYPE_PAGE_START && cm.dlc == 2) {
                uint16_t page = cm.data[0] << 8 | cm.data[1];

                h9frame_t cm_res;
                cm_res.type = H9FRAME_TYPE_PAGE_FILL_NEXT;
                cm_res.source_id = can_node_id;
                cm_res.unicast.flags = H9FRAME_FLAG_SINGE_MSG;
                cm_res.unicast.destination_id = cm.source_id;
                cm_res.unicast.seqnum = seqnum++;
                cm_res.dlc = 2;
                cm_res.data[0] = (SPM_PAGESIZE >> 8) & 0xff;
                cm_res.data[1] = (SPM_PAGESIZE) & 0xff;

                CAN_put_msg_blocking(&cm_res);

                write_page(page, cm.source_id);
            }
            if (cm.type == H9FRAME_TYPE_QUIT_BOOTLOADER && cm.dlc == 0) {
                MCUCR &= ~(1 << IVSEL);
                __asm__ volatile ("jmp  0x0000");
            }
        }
        else {
            turn_on_msg.unicast.seqnum = seqnum++;
            CAN_put_msg_blocking(&turn_on_msg);
        }
    }
}

