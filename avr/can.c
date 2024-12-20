// SPDX-License-Identifier: MIT
/*
 * H9 CAN protocol implementation for AVR
 *
 * Copyright (C) 2017-2024 Kamil Pałkowski
 *
 */

#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include <h9def.h>

#include "avr/can.h"

#define CAN_RX_BUF_SIZE 16
#define CAN_RX_BUF_INDEX_MASK 0x0F

#define CAN_TX_BUF_SIZE 8
#define CAN_TX_BUF_INDEX_MASK 0x07

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

typedef struct {
    uint8_t canidt1;
    uint8_t canidt2;
    uint8_t canidt3;
    uint8_t canidt4;
    uint8_t cancdmob;
    uint8_t data[8];
} can_buf_t;

static can_buf_t can_rx_buf[CAN_RX_BUF_SIZE];
static volatile uint8_t can_rx_buf_top = 0;
static volatile uint8_t can_rx_buf_bottom = 0;

static can_buf_t can_tx_buf[CAN_TX_BUF_SIZE];
static volatile uint8_t can_tx_buf_top = 0;
static volatile uint8_t can_tx_buf_bottom = 0;

volatile uint16_t can_node_id;
static uint8_t reset_reason __attribute__ ((section (".noinit")));
static uint16_t ee_node_id __attribute__((section(".eepromfixed"))) = H9MSG_BROADCAST_ID - 1;

static struct {
    uint16_t node_type;
    char hardware_revision;
    uint16_t version_major;
    uint16_t version_minor;
    char build_info[H9MSG_MAX_REGISTER_SIZE];
} node_info;

static void read_node_id(void);
static void write_node_id(uint16_t id);
static uint8_t calc_can_id1(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id);
static uint8_t calc_can_id2(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id);
static uint8_t calc_can_id3(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id);
static uint8_t calc_can_id4(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id);
static void set_CAN_id(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id);
static void set_CAN_id_mask(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id);

/* for software reset */
__attribute__((naked)) __attribute__((section(".init3"))) void wdt_init(void) {
    if (MCUSR == ((1 << PORF) | (1 << BORF))) reset_reason = NODE_RESET_BY_POWER_ON;
    else if (MCUSR == (1 << WDRF)) reset_reason = NODE_RESET_BY_WATCHDOG;
    else if (MCUSR == (1 << BORF)) reset_reason = NODE_RESET_BY_BROWN_OUT;
    else if (MCUSR == (1 << EXTRF)) reset_reason = NODE_RESET_BY_EXTERNAL_SOURCE;
    else reset_reason = NODE_RESET_BY_UNKNOW;

    MCUSR = 0;
    wdt_disable();
    return;
}


#if defined (__AVR_AT90CAN128__)
ISR(CANIT_vect) {
#else
ISR(CAN_INT_vect) {
#endif
    uint8_t canhpmob = CANHPMOB;
    uint8_t cangit = CANGIT;
    if (canhpmob != 0xf0) {
        uint8_t savecanpage = CANPAGE;
        CANPAGE = canhpmob;
        if (CANSTMOB & (1 << RXOK)) {
            can_rx_buf[can_rx_buf_top].canidt1 = CANIDT1;
            can_rx_buf[can_rx_buf_top].canidt2 = CANIDT2;
            can_rx_buf[can_rx_buf_top].canidt3 = CANIDT3;
            can_rx_buf[can_rx_buf_top].canidt4 = CANIDT4;
            can_rx_buf[can_rx_buf_top].cancdmob = CANCDMOB & 0x1f;
            for (uint8_t i = 0; i < 8; ++i) {
                can_rx_buf[can_rx_buf_top].data[i] = CANMSG;
            }
            can_rx_buf_top = (uint8_t)((can_rx_buf_top + 1) & CAN_RX_BUF_INDEX_MASK);
            CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob
            CANSTMOB = 0x00;  // Reset reason on selected channel
        }
        else if (CANSTMOB & (1 << TXOK)) {
            CANCDMOB = 0; //disable mob
            CANSTMOB = 0x00;  // Reset reason on selected channel
            if (can_tx_buf_top != can_tx_buf_bottom) {
                CANIDT1 = can_tx_buf[can_tx_buf_bottom].canidt1;
                CANIDT2 = can_tx_buf[can_tx_buf_bottom].canidt2;
                CANIDT3 = can_tx_buf[can_tx_buf_bottom].canidt3;
                CANIDT4 = can_tx_buf[can_tx_buf_bottom].canidt4;

                uint8_t idx = 0;
                for (; idx < 8; ++idx)
                    CANMSG = can_tx_buf[can_tx_buf_bottom].data[idx];

                CANCDMOB = (1 << CONMOB0) | (1 << IDE) | (can_tx_buf[can_tx_buf_bottom].cancdmob & 0x0f);

                can_tx_buf_bottom = (uint8_t)((can_tx_buf_bottom + 1) & CAN_TX_BUF_INDEX_MASK);
            }
        }
        else {
            CANSTMOB = 0x00;  // Reset reason on selected channel
        }
        CANPAGE = savecanpage;
    }
    //other interrupt
    CANGIT |= (cangit & 0x7f);
}


uint8_t process_msg(h9msg_t *cm) {
    if ((cm->type & H9MSG_NODE_STANDARD_MSG_BROADCAST_SUBGROUP_MASK) == H9MSG_NODE_STANDARD_MSG_BROADCAST_SUBGROUP
             && (cm->destination_id == can_node_id || cm->destination_id == H9MSG_BROADCAST_ID)) {
        if (cm->type == H9MSG_TYPE_DISCOVER && cm->dlc == 0) {
            h9msg_t cm_res;
            CAN_init_response_msg(cm, &cm_res);
            cm_res.dlc = 7;
            cm_res.data[0] = (node_info.node_type >> 8) & 0xff;
            cm_res.data[1] = (node_info.node_type) & 0xff;
            cm_res.data[2] = (node_info.version_major >> 8);
            cm_res.data[3] = node_info.version_major & 0xff;
            cm_res.data[4] = (node_info.version_minor >> 8) & 0xff;
            cm_res.data[5] = node_info.version_minor & 0xff;
            cm_res.data[6] = node_info.hardware_revision;
//            cm_res.data[7] = mcusr_mirror;
            CAN_put_msg(&cm_res);
            return 0;
        }
        else if (cm->type == H9MSG_TYPE_NODE_RESET && cm->dlc == 0) {
            cli();
            do {
                wdt_enable(WDTO_15MS);
                for(;;) {
                }
            } while(0);
        }
    }
    else if ((cm->type & H9MSG_NODE_STANDARD_MSG_GROUP_MASK) == H9MSG_NODE_STANDARD_MSG_GROUP && cm->destination_id == can_node_id) {
        if (cm->type == H9MSG_TYPE_SET_REG && cm->dlc > 1) {
            if (cm->data[0] >= 10)
                return 1;
            h9msg_t cm_res;
            CAN_init_response_msg(cm, &cm_res);
            cm_res.dlc = 2;
            cm_res.data[0] = cm->data[0];
            if (cm_res.data[0] == NODE_ID_STD_REGISTER) {
                if (cm->dlc == 3) {
                    write_node_id((cm->data[1] & 0x01) << 8 | cm->data[2]);

                    cm_res.data[1] = (can_node_id >> 8) & 0x01;
                    cm_res.data[2] = (can_node_id) & 0xff;
                    cm_res.dlc = 3;
                }
                else {
                    cm_res.type = H9MSG_TYPE_ERROR;
                    cm_res.data[0] = H9MSG_ERROR_REGISTER_SIZE_MISMATCH;
                    cm_res.dlc = 1;
                }
            }
            else if (cm_res.data[0] < NODE_STD_REGISTER_LAST) {
                cm_res.type = H9MSG_TYPE_ERROR;
                cm_res.data[0] = H9MSG_ERROR_READ_ONLY_REGISTER;
                cm_res.dlc = 1;
            }
            else {
                cm_res.type = H9MSG_TYPE_ERROR;
                cm_res.data[0] = H9MSG_ERROR_INVALID_REGISTER;
                cm_res.dlc = 1;
            }
            CAN_put_msg(&cm_res);
            return 0;
        }
        else if (cm->type == H9MSG_TYPE_GET_REG && cm->dlc == 1) {
            if (cm->data[0] >= 10)
                return 1;
            h9msg_t cm_res;
            CAN_init_response_msg(cm, &cm_res);
            cm_res.dlc = 2;
            cm_res.data[0] = cm->data[0];
            switch (cm_res.data[0]) {
                case NODE_TYPE_STD_REGISTER:
                    cm_res.data[1] = (node_info.node_type >> 8) & 0xff;
                    cm_res.data[2] = (node_info.node_type ) & 0xff;
                    cm_res.dlc = 3;
                    break;
                case NODE_HARDWARE_REVISION_STD_REGISTER:
                    cm_res.data[1] = 'a';
                    cm_res.dlc = 2;
                    break;
                case NODE_VERSION_STD_REGISTER:
                    cm_res.data[1] = (node_info.version_major >> 8);
                    cm_res.data[2] = node_info.version_major & 0xff;
                    cm_res.data[3] = (node_info.version_minor >> 8) & 0xff;
                    cm_res.data[4] = node_info.version_minor & 0xff;
                    cm_res.dlc = 5;
                    break;
                case NODE_BUILD_INFO_STD_REGISTER:
                //TODO: add multi-message value with message counter on 7 byte
                    cm_res.dlc = 7;
                    char *tmp = node_info.build_info;
                    uint8_t i = 0;
                    for (; tmp[i] && i < 6; ++i) {
                        cm_res.data[1 + i] = tmp[i];
                    }
                    for (; i < 6; ++i) {
                        cm_res.data[1 + i] = '\0';
                    }
                    break;
                case NODE_ID_STD_REGISTER:
                    cm_res.data[1] = (can_node_id >> 8) & 0x01;
                    cm_res.data[2] = (can_node_id) & 0xff;
                    cm_res.dlc = 3;
                    break;
                case NODE_MCU_TYPE_STD_REGISTER:
#if defined (__AVR_ATmega16M1__)
                    cm_res.data[1] = NODE_MCU_ATMEGA16M1;
#elif defined (__AVR_ATmega32M1__)
                    cm_res.data[1] = NODE_MCU_ATMEGA32M1;
#elif defined (__AVR_ATmega64M1__)
                    cm_res.data[1] = NODE_MCU_ATMEGA64M1;
#elif defined (__AVR_AT90CAN128__)
                    cm_res.data[1] = NODE_MCU_AT90CAN128;
#elif defined (__AVR_ATmega32C1__)
                    cm_res.data[2] = NODE_MCU_ATMEGA32C1;
#else
#error Unsupported MCU
#endif
                    cm_res.dlc = 2;
                    break;
                case NODE_SN_STD_REGISTER: //CPU serial ID
                    cm_res.data[1] = 0;
                    cm_res.data[2] = 0;
                    cm_res.data[3] = 0;
                    cm_res.data[4] = 0;
                    cm_res.dlc = 5;
                case NODE_RESET_REASON_STD_REGISTER:
                    cm_res.data[1] = reset_reason;
                    cm_res.dlc = 2;
                    break;
                default:
                    cm_res.type = H9MSG_TYPE_ERROR;
                    cm_res.data[0] = H9MSG_ERROR_INVALID_REGISTER;
                    cm_res.dlc = 1;
            }
            CAN_put_msg(&cm_res);
            return 0;
        }
        else if (cm->type == H9MSG_TYPE_NODE_UPGRADE && cm->dlc == 0) {
#ifdef BOOTSTART
            cli();
            asm volatile ( "jmp " STR(BOOTSTART) );
#else
            #warning "Node upgrade (bootloader) disable"
            h9msg_t cm_res;
            CAN_init_response_msg(cm, &cm_res);
            cm_res.type = H9MSG_TYPE_ERROR;
            cm_res.data[0] = H9MSG_ERROR_BOOTLOADER_UNSUPPORTED;
            cm_res.dlc = 1;
            CAN_put_msg(&cm_res);
            return 0;
#endif //BOOTSTART
        }
        else if (cm->type == H9MSG_TYPE_SET_BIT && cm->dlc == 2) {
            return 1;
        }
        else if (cm->type == H9MSG_TYPE_CLEAR_BIT && cm->dlc == 2) {
            return 1;
        }
        else if (cm->type == H9MSG_TYPE_TOGGLE_BIT && cm->dlc == 2) {
            return 1;
        }
    }
    else if ((cm->type & H9MSG_NODE_ALL_REMOTE_MSG_GROUP_MASK) == H9MSG_NODE_ALL_REMOTE_MSG_GROUP) {
        return 2;
    }

    h9msg_t cm_res;
    CAN_init_response_msg(cm, &cm_res);
    cm_res.type = H9MSG_TYPE_ERROR;
    cm_res.data[0] = H9MSG_ERROR_INVALID_MSG;
    cm_res.dlc = 1;
    CAN_put_msg(&cm_res);
    return 0;
}


void CAN_init(uint16_t node_type, char hardware_rev, uint16_t version_major, uint16_t version_minor, const char *build_info) {
    node_info.node_type = node_type;
    node_info.hardware_revision = hardware_rev;
    node_info.version_major = version_major;
    node_info.version_minor = version_minor;
    strncpy(node_info.build_info, build_info, H9MSG_MAX_REGISTER_SIZE);

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

    //select mob 1 for broadcast with type form 3rd group
    CANPAGE = 0x01 << MOBNB0;
    set_CAN_id(0, H9MSG_NODE_STANDARD_MSG_BROADCAST_SUBGROUP, 0, H9MSG_BROADCAST_ID, 0);
    set_CAN_id_mask(0, H9MSG_NODE_STANDARD_MSG_BROADCAST_SUBGROUP_MASK, 0, (1<<H9MSG_ID_BIT_LENGTH)-1, 0);
    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    //select mob 2 for unicast
    CANPAGE = 0x02 << MOBNB0;
    set_CAN_id(0, H9MSG_NODE_STANDARD_MSG_GROUP, 0, can_node_id, 0);
    set_CAN_id_mask(0, H9MSG_NODE_STANDARD_MSG_GROUP_MASK, 0, (1<<H9MSG_ID_BIT_LENGTH)-1, 0); //H9MSG_TYPE_GROUP_2 | H9MSG_TYPE_GROUP_3
    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only


    CANIE2 = ( 1 << IEMOB0 ) | ( 1 << IEMOB1 ) | ( 1 << IEMOB2 ); //interupt mob 0 1 and 2

    CANGIE = (1<<ENBOFF) | (1<<ENIT) | (1<<ENRX) | (1<<ENTX) | (1<<ENERR) | (1<<ENBX) | (1<<ENERG);
    CANGCON = 1<<ENASTB;
}


void CAN_send_turned_on_broadcast(void) {
    h9msg_t cm;
    CAN_init_new_msg(&cm);

    cm.type = H9MSG_TYPE_NODE_TURNED_ON;
    cm.destination_id = H9MSG_BROADCAST_ID;
    cm.dlc = 8;
    cm.data[0] = (node_info.node_type >> 8) & 0xff;
    cm.data[1] = (node_info.node_type) & 0xff;
    cm.data[2] = (node_info.version_major >> 8);
    cm.data[3] = node_info.version_major & 0xff;
    cm.data[4] = (node_info.version_minor >> 8) & 0xff;
    cm.data[5] = node_info.version_minor & 0xff;
    cm.data[6] = node_info.hardware_revision;
    cm.data[7] = reset_reason;
    CAN_put_msg(&cm);
}


void CAN_set_mob_for_remote_node1(uint16_t remote_node_id, uint8_t all_msg_group) {
    CANPAGE = 0x03 << MOBNB0; //select mob 3
    if (all_msg_group) {
        set_CAN_id(0, H9MSG_NODE_ALL_REMOTE_MSG_GROUP, 0, 0, remote_node_id);
        set_CAN_id_mask(0, H9MSG_NODE_ALL_REMOTE_MSG_GROUP_MASK, 0, 0, (1<<H9MSG_ID_BIT_LENGTH)-1);
    }
    else {
        set_CAN_id(0, H9MSG_NODE_RESPONSE_MSG_GROUP, 0, 0, remote_node_id);
        set_CAN_id_mask(0, H9MSG_NODE_RESPONSE_MSG_GROUP_MASK, 0, 0, (1<<H9MSG_ID_BIT_LENGTH)-1);
    }
    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    CANIE2 |= 1 << IEMOB3;
}


void CAN_set_mob_for_remote_node2(uint16_t remote_node_id, uint8_t all_msg_group) {
    CANPAGE = 0x04 << MOBNB0; //select mob 4
    if (all_msg_group) {
        set_CAN_id(0, H9MSG_NODE_ALL_REMOTE_MSG_GROUP, 0, 0, remote_node_id);
        set_CAN_id_mask(0, H9MSG_NODE_ALL_REMOTE_MSG_GROUP_MASK, 0, 0, (1<<H9MSG_ID_BIT_LENGTH)-1);
    }
    else {
        set_CAN_id(0, H9MSG_NODE_RESPONSE_MSG_GROUP, 0, 0, remote_node_id);
        set_CAN_id_mask(0, H9MSG_NODE_RESPONSE_MSG_GROUP_MASK, 0, 0, (1<<H9MSG_ID_BIT_LENGTH)-1);
    }
    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only
    
    CANIE2 |= 1 << IEMOB4;
}


void CAN_set_mob_for_remote_node3(uint16_t remote_node_id, uint8_t all_msg_group) {
    CANPAGE = 0x05 << MOBNB0; //select mob 5
    if (all_msg_group) {
        set_CAN_id(0, H9MSG_NODE_ALL_REMOTE_MSG_GROUP, 0, 0, remote_node_id);
        set_CAN_id_mask(0, H9MSG_NODE_ALL_REMOTE_MSG_GROUP_MASK, 0, 0, (1<<H9MSG_ID_BIT_LENGTH)-1);
    }
    else {
        set_CAN_id(0, H9MSG_NODE_RESPONSE_MSG_GROUP, 0, 0, remote_node_id);
        set_CAN_id_mask(0, H9MSG_NODE_RESPONSE_MSG_GROUP_MASK, 0, 0, (1<<H9MSG_ID_BIT_LENGTH)-1);
    }
    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only
    
    CANIE2 |= 1 << IEMOB5;
}


uint8_t CAN_try_put_msg(h9msg_t *cm) {
    CANPAGE = 0 << MOBNB0;              // Select MOb0 for transmission

    if (CANEN2 & ( 1 << ENMOB0 )) {
        return 0;
    }
    
    CANSTMOB = 0x00;                    // Clear mob status register

    set_CAN_id(cm->priority, cm->type, cm->seqnum, cm->destination_id, cm->source_id);

    for (uint8_t idx = 0; idx < cm->dlc; ++idx)
        CANMSG = cm->data[idx];

    CANCDMOB = (1 << CONMOB0) | (1 << IDE) | (cm->dlc & 0x0f);
    return 1;
}

uint8_t CAN_put_msg(h9msg_t *cm) {
    cli();
    uint8_t ret = 0;
    if (CAN_try_put_msg(cm)) {
        ret = 1;
    }
    else {
        uint8_t tmp_idx = (uint8_t) ((can_tx_buf_top + 1) & CAN_TX_BUF_INDEX_MASK);

        if (can_tx_buf_bottom != tmp_idx) {
            can_tx_buf[can_tx_buf_top].canidt1 = calc_can_id1(cm->priority, cm->type, cm->seqnum, cm->destination_id, cm->source_id);
            can_tx_buf[can_tx_buf_top].canidt2 = calc_can_id2(cm->priority, cm->type, cm->seqnum, cm->destination_id, cm->source_id);
            can_tx_buf[can_tx_buf_top].canidt3 = calc_can_id3(cm->priority, cm->type, cm->seqnum, cm->destination_id, cm->source_id);
            can_tx_buf[can_tx_buf_top].canidt4 = calc_can_id4(cm->priority, cm->type, cm->seqnum, cm->destination_id, cm->source_id);

            for (uint8_t idx = 0; idx < cm->dlc; ++idx)
                can_tx_buf[can_tx_buf_top].data[idx] = cm->data[idx];

            can_tx_buf[can_tx_buf_top].cancdmob = cm->dlc & 0x0f;

            can_tx_buf_top = tmp_idx;
            ret = 2;
        }
    }
    sei();
    return ret;
}

uint8_t CAN_get_msg(h9msg_t *cm) {
    if (can_rx_buf_top != can_rx_buf_bottom) {
        cm->priority = (can_rx_buf[can_rx_buf_bottom].canidt1 >> 7) & 0x01;
        cm->type = ((can_rx_buf[can_rx_buf_bottom].canidt1 >> 2) & 0x1f);
        cm->seqnum = ((can_rx_buf[can_rx_buf_bottom].canidt1 << 3) & 0x18) | ((can_rx_buf[can_rx_buf_bottom].canidt2 >> 5) & 0x07);
        cm->destination_id = ((can_rx_buf[can_rx_buf_bottom].canidt2 << 4) & 0x1f0) | ((can_rx_buf[can_rx_buf_bottom].canidt3 >> 4) & 0x0f);
        cm->source_id = ((can_rx_buf[can_rx_buf_bottom].canidt3 << 5) & 0x1e0) | ((can_rx_buf[can_rx_buf_bottom].canidt4 >> 3) & 0x1f);

        cm->dlc = can_rx_buf[can_rx_buf_bottom].cancdmob & 0x0f;
        uint8_t idx = 0;
        for (; idx < 8; ++idx)
            cm->data[idx] = can_rx_buf[can_rx_buf_bottom].data[idx];

        can_rx_buf_bottom = (uint8_t)((can_rx_buf_bottom + 1) & CAN_RX_BUF_INDEX_MASK);

        // 1st msg filter: mob filter/mask
        // 2nd msg filter
        if (cm->source_id == H9MSG_BROADCAST_ID) { //invalid message, drop
            return 0;
        }

        return process_msg(cm);
    }
    return 0;
}


void CAN_init_new_msg(h9msg_t *mes) {
    static uint8_t next_seqnum = 0;
    mes->priority = H9MSG_PRIORITY_LOW;
    mes->seqnum = next_seqnum;
    mes->source_id = can_node_id;
    mes->dlc = 0;
    ++next_seqnum;
}


void CAN_init_response_msg(const h9msg_t *req, h9msg_t *res) {
    res->priority = req->priority;
    res->seqnum = req->seqnum;
    switch (req->type) {
        case H9MSG_TYPE_GET_REG:
            res->type = H9MSG_TYPE_REG_VALUE;
            break;
        case H9MSG_TYPE_SET_REG:
        case H9MSG_TYPE_SET_BIT:
        case H9MSG_TYPE_CLEAR_BIT:
        case H9MSG_TYPE_TOGGLE_BIT:
            res->type = H9MSG_TYPE_REG_EXTERNALLY_CHANGED;
            break;
        case H9MSG_TYPE_DISCOVER:
            res->type = H9MSG_TYPE_NODE_INFO;
            break;
    }
    res->source_id = can_node_id;
    res->destination_id = req->source_id;
    res->dlc = 0;
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


void write_node_id(uint16_t id) {
    cli();
    eeprom_write_word(&ee_node_id, id);
    sei();
}

static uint8_t calc_can_id1(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id) {
    return ((priority << 7) & 0x80) | ((type << 2) & 0x7c) | ((seqnum >> 3) & 0x03);
}

static uint8_t calc_can_id2(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id) {
    return ((seqnum << 5) & 0xe0) | ((destination_id >> 4) & 0x1f);
}

static uint8_t calc_can_id3(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id) {
    return ((destination_id << 4) & 0xf0) | ((source_id >> 5) & 0x0f);
}

static uint8_t calc_can_id4(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id) {
    return ((source_id << 3) & 0xf8);
}


void set_CAN_id(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id) {
    CANIDT1 = calc_can_id1(priority, type, seqnum, destination_id, source_id);
    CANIDT2 = calc_can_id2(priority, type, seqnum, destination_id, source_id);
    CANIDT3 = calc_can_id3(priority, type, seqnum, destination_id, source_id);
    CANIDT4 = calc_can_id4(priority, type, seqnum, destination_id, source_id);
}


void set_CAN_id_mask(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id) {
    CANIDM1 = calc_can_id1(priority, type, seqnum, destination_id, source_id);
    CANIDM2 = calc_can_id2(priority, type, seqnum, destination_id, source_id);
    CANIDM3 = calc_can_id3(priority, type, seqnum, destination_id, source_id);
    CANIDM4 = calc_can_id4(priority, type, seqnum, destination_id, source_id);
}
