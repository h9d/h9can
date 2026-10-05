// SPDX-License-Identifier: MIT
/*
 * H9 CAN bootloader for AVR
 *
 * Copyright (C) 2020-2024 Kamil Pałkowski
 *
 */

#include "can.h"
#include <h9avr/node_id.h>

uint8_t can_node_id;
uint16_t can_node_type;

static void set_CAN_unicast_id(uint8_t type, uint8_t src, uint8_t flags, uint8_t dst, uint8_t seq);
static void set_CAN_unicast_id_mask(uint8_t type, uint8_t src, uint8_t flags, uint8_t dst, uint8_t seq);
static void calc_can_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, h9frame_t *cm);
static void set_CAN_id(h9frame_t *cm);

void CAN_init(void) {
    can_node_id = read_node_id(&can_node_type);

    CANGCON = ( 1 << SWRES );   // Software reset
    CANTCON = 0x00;             // CAN timing prescaler set to 0;

    #if F_CPU == 4000000UL
        CANBT1 = 0x06;
        CANBT2 = 0x04;
        CANBT3 = 0x13;
    #elif F_CPU == 12000000UL
        CANBT1 = 0x16;
        CANBT2 = 0x04;
        CANBT3 = 0x13;
    #elif F_CPU == 16000000UL
        CANBT1 = 0x1e;
        CANBT2 = 0x04;
        CANBT3 = 0x13;
    #else
        #error "Please specify F_CPU"
    #endif

    for ( int8_t mob=0; mob<6; mob++ ) {
        CANPAGE = ( mob << MOBNB0 ); // Selects Message Object 0-5
        CANCDMOB = 0x00;             // Disable mob
        CANSTMOB = 0x00;             // Clear mob status register;
    }


    // 1st msg filter
    CANPAGE = 0x01 << MOBNB0;
    set_CAN_unicast_id(H9FRAME_BOOTLOADER_MSG_TYPE_GROUP, 0, 0, can_node_id, 0);
    set_CAN_unicast_id_mask(H9FRAME_BOOTLOADER_MSG_TYPE_GROUP_MASK, 0, 0, H9FRAME_ID_MASK, 0);
    CANIDM4 |= 1 << IDEMSK;
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    CANGCON = 1<<ENASTB;
}


void CAN_put_msg_blocking(h9frame_t *cm) {
    CANPAGE = 0 << MOBNB0;              // Select MOb0 for transmission
    while ( CANEN2 & ( 1 << ENMOB0 ) ); // Wait for MOb 0 to be free
    CANSTMOB = 0x00;                    // Clear mob status register

    set_CAN_id(cm);

    uint8_t idx = 0;
    for (; idx < 8; ++idx)
        CANMSG = cm->data[idx];

    CANCDMOB = (1 << CONMOB0) | (1 << IDE) | (cm->dlc & 0x0f);
}


uint8_t CAN_get_msg_blocking(h9frame_t *cm) {
    uint32_t timeout_counter = 0x1fffff;

    while (timeout_counter) {
        CANPAGE = 0x01 << MOBNB0;
        if (CANSTMOB & (1 << RXOK)) {

            uint8_t canidt1 = CANIDT1;
            uint8_t canidt2 = CANIDT2;
            uint8_t canidt3 = CANIDT3;
            uint8_t canidt4 = CANIDT4;
            uint8_t cancdmob = CANCDMOB & 0x1f;

            for (uint8_t i = 0; i < 8; ++i) {
                cm->data[i] = CANMSG;
            }

            cm->type = canidt1 >> 3;
            cm->source_id = (canidt1 << 5) | (canidt2 >> 3);
            cm->unicast.flags = ((canidt2) & 0x03);
            cm->unicast.destination_id  = canidt3;
            cm->unicast.seqnum = canidt4 >> 3;

            cm->dlc = cancdmob & 0x0f;

            CANCDMOB = (1 << CONMOB1) | (1 << IDE); //rx mob
            CANSTMOB = 0x00;
            return 1;
        }
        --timeout_counter;
    }
    return 0;
}

static void calc_can_unicast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t src, uint8_t flags, uint8_t dst, uint8_t seq) {
    *id1 = (type << 3) | (src >> 5);
    *id2 = (src << 3) | (flags & 0x03);
    *id3 = dst;
    *id4 = ((seq << 3) & 0xf8);
}

static void calc_can_broadcast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t src, uint16_t node_type) {
    *id1 = (type << 3) | (src >> 5);
    *id2 = (src << 3) | ((node_type >> 13) & 0x03);
    *id3 = ((node_type >> 5) & 0xff);
    *id4 = ((node_type << 3) & 0xf8);
}

static void calc_can_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, h9frame_t *cm) {
    if (cm->type & H9FRAME_UNICAST_BROADCAST_BIT)
        calc_can_broadcast_id(id1, id2, id3, id4, cm->type, cm->source_id, cm->broadcast.group);
    else
        calc_can_unicast_id(id1, id2, id3, id4, cm->type, cm->source_id, cm->unicast.flags, cm->unicast.destination_id, cm->unicast.seqnum);
}

static void set_CAN_id(h9frame_t *cm) {
    calc_can_id(&CANIDT1, &CANIDT2, &CANIDT3, &CANIDT4, cm);
}

static void set_CAN_unicast_id(uint8_t type, uint8_t src, uint8_t flags, uint8_t dst, uint8_t seq) {
    calc_can_unicast_id(&CANIDT1, &CANIDT2, &CANIDT3, &CANIDT4, type & 0x0f, src, flags, dst, seq);
}

static void set_CAN_unicast_id_mask(uint8_t type, uint8_t src, uint8_t flags, uint8_t dst, uint8_t seq) {
    calc_can_unicast_id(&CANIDM1, &CANIDM2, &CANIDM3, &CANIDM4, type | 0x10, src, flags, dst, seq);
}
