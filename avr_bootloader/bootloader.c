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
#include "../include/h9msg.h"
#include "can.h"

static uint8_t seqnum = 0;

void write_page(uint16_t page, uint16_t dst_id) {
    uint16_t bytes_remain = SPM_PAGESIZE;
    page = page * SPM_PAGESIZE;

    boot_page_erase_safe(page);
    while (1) {
        h9msg_t cm;
        CAN_get_msg_blocking(&cm);

        h9msg_t cm_res;

        cm_res.priority = H9MSG_PRIORITY_HIGH;
        cm_res.source_id = can_node_id;
        cm_res.destination_id = dst_id;
        cm_res.seqnum = cm.seqnum;

        if (cm.source_id == dst_id && cm.type == H9MSG_TYPE_PAGE_FILL && cm.dlc == 8) {
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
                cm_res.type = H9MSG_TYPE_PAGE_WRITED;
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
                cm_res.type = H9MSG_TYPE_PAGE_FILL_NEXT;
                cm_res.dlc = 2;
                cm_res.data[0] = (bytes_remain >> 8) & 0xff;
                cm_res.data[1] = (bytes_remain) & 0xff;
                CAN_put_msg_blocking(&cm_res);
            }
        }
        else if (cm.source_id == dst_id && (cm.type & H9MSG_BOOTLOADER_MSG_GROUP_MASK) == H9MSG_BOOTLOADER_MSG_GROUP) {
            cm_res.type = H9MSG_TYPE_PAGE_FILL_BREAK;
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
    
    h9msg_t turn_on_msg;

    turn_on_msg.type = H9MSG_TYPE_BOOTLOADER_TURNED_ON;
    turn_on_msg.priority = H9MSG_PRIORITY_HIGH;
    turn_on_msg.source_id = can_node_id;
    turn_on_msg.destination_id = H9MSG_BROADCAST_ID;
    turn_on_msg.seqnum = seqnum++;
//    turn_on_msg.dlc = 5;
    turn_on_msg.dlc = 4;
    turn_on_msg.data[0] = BOOTLOADER_VERSION_MAJOR;
    turn_on_msg.data[1] = BOOTLOADER_VERSION_MINOR;
#if defined (__AVR_ATmega16M1__)
    turn_on_msg.data[2] = NODE_MCU_ATMEGA16M1;
#elif defined (__AVR_ATmega32M1__)
    turn_on_msg.data[2] = NODE_MCU_ATMEGA32M1;
#elif defined (__AVR_ATmega64M1__)
    turn_on_msg.data[2] = NODE_MCU_ATMEGA64M1;
#elif defined (__AVR_AT90CAN128__)
    turn_on_msg.data[2] = NODE_MCU_AT90CAN128;
#elif defined (__AVR_ATmega32C1__)
    turn_on_msg.data[2] = NODE_MCU_ATMEGA32C1;
#else
#error "Unsupported MCU"
#endif

#if F_CPU == 4000000UL
    turn_on_msg.data[3] = NODE_MCU_F_4MHz;
#elif F_CPU == 12000000UL
    turn_on_msg.data[3] = NODE_MCU_F_12MHz;
#elif F_CPU == 16000000UL
    turn_on_msg.data[3] = NODE_MCU_F_16MHz;
#else
#error "Please specify F_CPU"
#endif
//    turn_on_msg.data[3] = (NODE_TYPE >> 8) & 0xff;
//    turn_on_msg.data[4] = (NODE_TYPE) & 0xff;
    CAN_put_msg_blocking(&turn_on_msg);
    
    while (1) {
        h9msg_t cm;
        if (CAN_get_msg_blocking(&cm)) {
            if (cm.type == H9MSG_TYPE_PAGE_START && cm.dlc == 2) {
                uint16_t page = cm.data[0] << 8 | cm.data[1];

                h9msg_t cm_res;
                cm_res.type = H9MSG_TYPE_PAGE_FILL_NEXT;
                cm_res.priority = H9MSG_PRIORITY_HIGH;
                cm_res.source_id = can_node_id;
                cm_res.destination_id = cm.source_id;
                cm_res.seqnum = seqnum++;
                cm_res.dlc = 2;
                cm_res.data[0] = (SPM_PAGESIZE >> 8) & 0xff;
                cm_res.data[1] = (SPM_PAGESIZE) & 0xff;

                CAN_put_msg_blocking(&cm_res);

                write_page(page, cm.source_id);
            }
            if (cm.type == H9MSG_TYPE_QUIT_BOOTLOADER && cm.dlc == 0) {
                MCUCR &= ~(1 << IVSEL);
                asm volatile ("jmp  0x0000");
            }
        }
        else {
            turn_on_msg.seqnum = seqnum++;
            CAN_put_msg_blocking(&turn_on_msg);
        }
    }
}

