// SPDX-License-Identifier: MIT
/*
 * H9 CAN bootloader for AVR
 *
 * Copyright (C) 2020-2024 Kamil Pałkowski
 *
 */

#include "can.h"


volatile uint16_t can_node_id;
uint16_t ee_node_id EEMEM = 0;

static void read_node_id(void);
static void set_CAN_id(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id);
static void set_CAN_id_mask(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id);


void CAN_init(void) {
    read_node_id();

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
    set_CAN_id(0, H9MSG_BOOTLOADER_MSG_GROUP, 0, can_node_id, 0);
    set_CAN_id_mask(0, H9MSG_BOOTLOADER_MSG_GROUP_MASK, 0, (1<<H9MSG_ID_BIT_LENGTH)-1, 0);
    CANIDM4 |= 1 << IDEMSK;
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    CANGCON = 1<<ENASTB;
}


void CAN_put_msg_blocking(h9msg_t *cm) {
    CANPAGE = 0 << MOBNB0;              // Select MOb0 for transmission
    while ( CANEN2 & ( 1 << ENMOB0 ) ); // Wait for MOb 0 to be free
    CANSTMOB = 0x00;                    // Clear mob status register

    set_CAN_id(cm->priority, cm->type, cm->seqnum, cm->destination_id, cm->source_id);

    uint8_t idx = 0;
    for (; idx < 8; ++idx)
        CANMSG = cm->data[idx];

    CANCDMOB = (1 << CONMOB0) | (1 << IDE) | (cm->dlc & 0x0f);
}


uint8_t CAN_get_msg_blocking(h9msg_t *cm) {
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

            cm->priority = (canidt1 >> 7) & 0x01;
            cm->type = (canidt1 >> 2) & 0x1f;
            cm->seqnum = ((canidt1 << 3) & 0x18) | ((canidt2 >> 5) & 0x07);
            cm->destination_id = ((canidt2 << 4) & 0x1f0) | ((canidt3 >> 4) & 0x0f);
            cm->source_id = ((canidt3 << 5) & 0x1e0) | ((canidt4 >> 3) & 0x1f);

            cm->dlc = cancdmob & 0x0f;

            CANCDMOB = (1 << CONMOB1) | (1 << IDE); //rx mob
            CANSTMOB = 0x00;
            return 1;
        }
        --timeout_counter;
    }
    return 0;
}


void read_node_id(void) {
    uint16_t node_id = eeprom_read_word(&ee_node_id);
    if (node_id > 0 && node_id < H9MSG_BROADCAST_ID) {
        can_node_id = node_id & ((1<<H9MSG_ID_BIT_LENGTH)-1);
    }
    else {
        can_node_id = 0;
    }
}


void set_CAN_id(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id) {
    CANIDT1 = ((priority << 7) & 0x80) | ((type << 2) & 0x7c) | ((seqnum >> 3) & 0x03);
    CANIDT2 = ((seqnum << 5) & 0xe0) | ((destination_id >> 4) & 0x1f);
    CANIDT3 = ((destination_id << 4) & 0xf0) | ((source_id >> 5) & 0x0f);
    CANIDT4 = ((source_id << 3) & 0xf8);
}


void set_CAN_id_mask(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id) {
    CANIDM1 = ((priority << 7) & 0x80) | ((type << 2) & 0x7c) | ((seqnum >> 3) & 0x03);
    CANIDM2 = ((seqnum << 5) & 0xe0) | ((destination_id >> 4) & 0x1f);
    CANIDM3 = ((destination_id << 4) & 0xf0) | ((source_id >> 5) & 0x0f);
    CANIDM4 = ((source_id << 3) & 0xf8);
}
