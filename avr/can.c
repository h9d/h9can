// SPDX-License-Identifier: MIT
/*
 * H9 CAN protocol implementation for AVR
 *
 * Copyright (C) 2017-2026 Kamil Pałkowski
 *
 */

#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include <h9def.h>

#include "avr/can.h"

#include <errno.h>

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

volatile uint8_t can_node_id;
static uint8_t reset_reason __attribute__ ((section (".noinit")));
static uint8_t ee_node_id __attribute__((section(".eepromfixed"))) = 0;

static struct {
    uint16_t node_type;
    char hardware_revision;
    uint16_t version_major;
    uint16_t version_minor;
    char build_info[H9MSG_MAX_REGISTER_SIZE];
} node_info;

static void read_node_id(void);
static void write_node_id(uint8_t id);

static void calc_can_id(volatile uint8_t* id1, volatile uint8_t* id2, volatile uint8_t* id3, volatile uint8_t* id4, h9msg_t* cm);
static void calc_can_unicast_id(volatile uint8_t* id1, volatile uint8_t* id2, volatile uint8_t* id3, volatile uint8_t* id4, uint8_t priority, uint8_t type, uint8_t flags, uint8_t src, uint8_t dst, uint8_t seq);
static void calc_can_broadcast_id(volatile uint8_t* id1, volatile uint8_t* id2, volatile uint8_t* id3, volatile uint8_t* id4, uint8_t priority, uint8_t type, uint8_t flags, uint8_t src, uint16_t node_type);

static void CAN_send_node_info_broadcast(uint8_t turn_on);

static void set_CAN_id(h9msg_t* cm);
static void set_CAN_unicast_id(uint8_t priority, uint8_t type, uint8_t flags, uint8_t src, uint8_t dst, uint8_t seq);
static void set_CAN_unicast_id_mask(uint8_t priority, uint8_t type, uint8_t flags, uint8_t src, uint8_t dst, uint8_t seq);
static void set_CAN_broadcast_id(uint8_t priority, uint8_t type, uint8_t flags, uint8_t src, uint16_t node_type);
static void set_CAN_broadcast_id_mask(uint8_t priority, uint8_t type, uint8_t flags, uint8_t src, uint16_t node_type);

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


static void __attribute__((noreturn)) mcu_reset(void) {
    cli();
    do {
        wdt_enable(WDTO_15MS);
        for(;;) {
        }
    } while(0);
}

void send_command_error(uint8_t errno, uint8_t destination, uint8_t seqnum) {
    h9msg_t cm;
    cm.priority = H9MSG_PRIORITY_LOW;
    cm.type = H9MSG_TYPE_COMMAND_ERROR;
    cm.flags = 0;
    cm.source_id = can_node_id;
    cm.destination_id = destination;
    cm.seqnum = seqnum;

    cm.data[0] = errno;
    cm.dlc = 1;
    CAN_put_msg(&cm);
}

void send_reg_value(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t* value, size_t length) {
    //TODO: add multi-message value with message counter on 7 byte
    h9msg_t cm;
    cm.priority = H9MSG_PRIORITY_LOW;
    cm.type = H9MSG_TYPE_REG_VALUE;
    cm.flags = 0;
    cm.source_id = can_node_id;
    cm.destination_id = destination;
    cm.seqnum = seqnum;

    length = length < 7 ? length : 7;

    uint8_t i = 0;
    for (; i < length; ++i) {
        cm.data[1 + i] = value[i];
    }

    cm.data[0] = registry;
    cm.dlc = length + 1;
    CAN_put_msg(&cm);
}

void send_reg_value1(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value) {
    h9msg_t cm;
    cm.priority = H9MSG_PRIORITY_LOW;
    cm.type = H9MSG_TYPE_REG_VALUE;
    cm.flags = 0;
    cm.source_id = can_node_id;
    cm.destination_id = destination;
    cm.seqnum = seqnum;

    cm.data[0] = registry;
    cm.data[1] = value;
    cm.dlc = 2;
    CAN_put_msg(&cm);
}

void send_reg_value2(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2) {
    h9msg_t cm;
    cm.priority = H9MSG_PRIORITY_LOW;
    cm.type = H9MSG_TYPE_REG_VALUE;
    cm.flags = 0;
    cm.source_id = can_node_id;
    cm.destination_id = destination;
    cm.seqnum = seqnum;

    // uint8_t tmp[] = {value1, value2};
    // send_reg_value(registry,destination,seqnum, tmp, 2);

    cm.data[0] = registry;
    cm.data[1] = value1;
    cm.data[2] = value2;
    cm.dlc = 3;
    CAN_put_msg(&cm);
}

void send_reg_value3(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3) {
    h9msg_t cm;
    cm.priority = H9MSG_PRIORITY_LOW;
    cm.type = H9MSG_TYPE_REG_VALUE;
    cm.flags = 0;
    cm.source_id = can_node_id;
    cm.destination_id = destination;
    cm.seqnum = seqnum;

    cm.data[0] = registry;
    cm.data[1] = value1;
    cm.data[2] = value2;
    cm.data[3] = value3;
    cm.dlc = 4;
    CAN_put_msg(&cm);
}

void send_reg_value4(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3, uint8_t value4) {
    h9msg_t cm;
    cm.priority = H9MSG_PRIORITY_LOW;
    cm.type = H9MSG_TYPE_REG_VALUE;
    cm.flags = 0;
    cm.source_id = can_node_id;
    cm.destination_id = destination;
    cm.seqnum = seqnum;

    cm.data[0] = registry;
    cm.data[1] = value1;
    cm.data[2] = value2;
    cm.data[3] = value3;
    cm.data[4] = value4;
    cm.dlc = 5;
    CAN_put_msg(&cm);
}

static void process_standard_reg(h9msg_t *cm) {
    if (cm->type == H9MSG_TYPE_SET_REG && cm->dlc > 1) {
        if (cm->data[0] == NODE_ID_STD_REGISTER) {
            if (cm->dlc == 3) {
                write_node_id((cm->data[1] & 0x01) << 8 | cm->data[2]);

                send_reg_value2(NODE_ID_STD_REGISTER, cm->source_id, cm->seqnum, (can_node_id >> 8) & 0x01, (can_node_id) & 0xff);
                return ;
            }
            else {
                send_command_error(H9FRAME_ERROR_REGISTER_SIZE_MISMATCH, cm->source_id, cm->seqnum);
                return;
            }
        }
        else if (cm->data[0] < NODE_STD_REGISTER_LAST) {
            send_command_error(H9FRAME_ERROR_READ_ONLY_REGISTER, cm->source_id, cm->seqnum);
            return;
        }
        else {
            send_command_error(H9FRAME_ERROR_INVALID_REGISTER, cm->source_id, cm->seqnum);
            return;
        }
    }
    else if (cm->type == H9MSG_TYPE_GET_REG && cm->dlc == 1) {
        switch (cm->data[0]) {
            case NODE_TYPE_STD_REGISTER:
                send_reg_value2(NODE_TYPE_STD_REGISTER, cm->source_id, cm->seqnum, (node_info.node_type >> 8) & 0xff, (node_info.node_type ) & 0xff);
                return;
            case NODE_HARDWARE_REVISION_STD_REGISTER:
                send_reg_value1(NODE_HARDWARE_REVISION_STD_REGISTER, cm->source_id, cm->seqnum, 'a');
                return;
            case NODE_VERSION_STD_REGISTER:
                send_reg_value4(NODE_VERSION_STD_REGISTER, cm->source_id, cm->seqnum, (node_info.version_major >> 8), node_info.version_major & 0xff, (node_info.version_minor >> 8) & 0xff, node_info.version_minor & 0xff);
                return;
            case NODE_BUILD_INFO_STD_REGISTER:
                //TODO: add multi-message value with message counter on 7 byte
                send_reg_value(NODE_BUILD_INFO_STD_REGISTER, cm->source_id, cm->seqnum, (uint8_t*)&node_info.build_info, H9MSG_MAX_REGISTER_SIZE);
                return;
            case NODE_ID_STD_REGISTER:
                send_reg_value2(NODE_ID_STD_REGISTER, cm->source_id, cm->seqnum, (can_node_id >> 8) & 0x01, (can_node_id) & 0xff);
                return;
            case NODE_MCU_TYPE_STD_REGISTER:
#if defined (__AVR_ATmega16M1__)
                send_reg_value1(NODE_MCU_TYPE_STD_REGISTER, cm->source_id, cm->seqnum, NODE_MCU_ATMEGA16M1);
#elif defined (__AVR_ATmega32M1__)
                send_reg_value1(NODE_MCU_TYPE_STD_REGISTER, cm->source_id, cm->seqnum, NODE_MCU_ATMEGA32M1);
#elif defined (__AVR_ATmega64M1__)
                send_reg_value1(NODE_MCU_TYPE_STD_REGISTER, cm->source_id, cm->seqnum, NODE_MCU_ATMEGA64M1);
#elif defined (__AVR_AT90CAN128__)
                send_reg_value1(NODE_MCU_TYPE_STD_REGISTER, cm->source_id, cm->seqnum, NODE_MCU_AT90CAN128);
#elif defined (__AVR_ATmega32C1__)
                send_reg_value1(NODE_MCU_TYPE_STD_REGISTER, cm->source_id, cm->seqnum, NODE_MCU_ATMEGA32C1);
#else
#error Unsupported MCU
#endif
                return;
            case NODE_SN_STD_REGISTER: //CPU serial ID
                send_reg_value4(NODE_SN_STD_REGISTER, cm->source_id, cm->seqnum, 0, 0, 0, 0);
                return;
            case NODE_RESET_REASON_STD_REGISTER:
                send_reg_value1(NODE_RESET_REASON_STD_REGISTER, cm->source_id, cm->seqnum, reset_reason);
                return;
            default:
                send_command_error(H9FRAME_ERROR_INVALID_REGISTER, cm->source_id, cm->seqnum);
                return;
        }
        return;
    }
    // else if (cm->type == H9MSG_TYPE_SET_BIT && cm->dlc == 2) {
    //     return ;
    // }
    // else if (cm->type == H9MSG_TYPE_CLEAR_BIT && cm->dlc == 2) {
    //     return ;
    // }

    send_command_error(H9FRAME_ERROR_UNSUPPORTED_OPERATION, cm->source_id, cm->seqnum);
}

uint8_t process_msg(h9msg_t *cm) {
    /* --- BROADCAST --- */
    if (cm->type & H9MSG_UNICAST_BROADCAST_BIT) {
        if (cm->type == H9MSG_TYPE_DISCOVER || cm->type == H9MSG_TYPE_GROUP_RESET) {
            if (cm->broadcast_group == node_info.node_type || cm->broadcast_group == H9MSG_BROADCAST_ID) {
                if (cm->type == H9MSG_TYPE_DISCOVER) {
                    CAN_send_node_info_broadcast(0);
                    return 0;
                }
                else if (cm->type == H9MSG_TYPE_GROUP_RESET) {
                    mcu_reset();
                    return 0;
                }
            }
            else {
                // INVALID_MSG but we don't answere on broadcast
                return 0;
            }
        }

        return 1;
    }
    /* --- UNICAST --- */
    else if (!(cm->type & H9MSG_UNICAST_BROADCAST_BIT)) {
        if (cm->destination_id != can_node_id) {
            return 0; //not for me
        }

        /* -- RCV BOOTLOADER MSG -- */
        if (cm->type <= H9MSG_TYPE_PAGE_FILL_BREAK) {
            send_command_error(H9FRAME_ERROR_INVALID_MSG, cm->source_id, cm->seqnum);
            return 0;
        }

        /* -- MULTIPLE MSG -- */
        if (cm->flags != 0 && cm->type != H9MSG_TYPE_SET_REG && cm->type != H9MSG_TYPE_REG_VALUE) {
            send_command_error(H9FRAME_ERROR_INVALID_MSG, cm->source_id, cm->seqnum);
            return 0;
        }

        if (cm->type == H9MSG_TYPE_NODE_RESET) {
            mcu_reset();
            return 0;
        }
        else if (cm->type == H9MSG_TYPE_NODE_UPGRADE && cm->dlc == 0) {
#ifdef BOOTSTART
            cli();
            asm volatile ( "jmp " STR(BOOTSTART) );
#else
#warning "Node upgrade (bootloader) disable"
            send_command_error(H9MSG_ERROR_BOOTLOADER_UNSUPPORTED, cm->source_id, cm->seqnum);
            return 0;
#endif //BOOTSTART
        }
        else if (cm->type == H9MSG_TYPE_SET_REG || cm->type == H9MSG_TYPE_GET_REG || cm->type == H9MSG_TYPE_SET_BIT || cm->type == H9MSG_TYPE_CLEAR_BIT) {
            /* --- STANDARD REG OPERATION -- */
            if (cm->dlc > 0 && cm->data[0] < 10) {
                process_standard_reg(cm);
            }
            else {
                return 1;
            }
        }
        return 1; //THEORETICALLY H9MSG_TYPE_COMMAND_ERROR OR H9MSG_TYPE_REG_VALUE
    }
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

    //select mob 1 for unicast
    CANPAGE = 0x01 << MOBNB0;
    set_CAN_unicast_id(0, H9MSG_UNICAST_MSG_TYPE_GROUP, 0, 0, can_node_id, 0);
    set_CAN_unicast_id_mask(0, H9MSG_UNICAST_MSG_TYPE_GROUP_MASK, 0, 0, H9MSG_ID_MASK, 0); //H9MSG_TYPE_GROUP_2 | H9MSG_TYPE_GROUP_3
    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    //select mob 2 for broadcast
    CANPAGE = 0x02 << MOBNB0;
    set_CAN_broadcast_id(0, H9MSG_SPECIAL_BROADCAST_MSG_TYPE_GROUP, 0, 0, 0);
    set_CAN_broadcast_id_mask(0, H9MSG_SPECIAL_BROADCAST_MSG_TYPE_GROUP_MASK, 0, 0, H9MSG_NODE_TYPE_MASK);
    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    CANPAGE = 0x03 << MOBNB0;
    set_CAN_broadcast_id(0, H9MSG_SPECIAL_BROADCAST_MSG_TYPE_GROUP, 0, 0, node_type);
    set_CAN_broadcast_id_mask(0, H9MSG_SPECIAL_BROADCAST_MSG_TYPE_GROUP_MASK, 0, 0, H9MSG_NODE_TYPE_MASK) ;
    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    CANIE2 = ( 1 << IEMOB0 ) | ( 1 << IEMOB1 ) | ( 1 << IEMOB2 ) | ( 1 << IEMOB3 ); //interupt mob 0 1 2 3

    CANGIE = (1<<ENBOFF) | (1<<ENIT) | (1<<ENRX) | (1<<ENTX) | (1<<ENERR) | (1<<ENBX) | (1<<ENERG);
    CANGCON = 1<<ENASTB;
}

void CAN_send_turned_on_broadcast(void) {
    CAN_send_node_info_broadcast(1);
}

static void CAN_send_node_info_broadcast(uint8_t turn_on) {
    h9msg_t cm;

    cm.priority = H9MSG_PRIORITY_LOW;
    if (turn_on)
        cm.type = H9MSG_TYPE_NODE_TURNED_ON;
    else
        cm.type = H9MSG_TYPE_NODE_INFO;
    cm.flags = 0;
    cm.source_id = can_node_id;
    cm.broadcast_group = node_info.node_type;

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


void CAN_set_msg_filter_1(uint8_t remote_node_id, uint8_t remote_node_id_active, uint16_t broadcast_group, uint8_t broadcast_group_active) {
    CANPAGE = 0x04 << MOBNB0; //select mob 4

    uint8_t node_id_mask = remote_node_id_active ? H9MSG_ID_MASK : 0;
    uint16_t node_type_mask = broadcast_group_active ? H9MSG_NODE_TYPE_MASK : 0;

    set_CAN_broadcast_id(0, H9MSG_ALL_BROADCAST_MSG_TYPE_GROUP, 0, remote_node_id, broadcast_group);
    set_CAN_broadcast_id_mask(0, H9MSG_ALL_BROADCAST_MSG_TYPE_GROUP_MASK, 0, node_id_mask, node_type_mask);

    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    CANIE2 |= 1 << IEMOB3;
}


void CAN_set_msg_filter_2(uint8_t remote_node_id, uint8_t remote_node_id_active, uint16_t broadcast_group, uint8_t broadcast_group_active){
    CANPAGE = 0x05 << MOBNB0; //select mob 5

    uint8_t node_id_mask = remote_node_id_active ? H9MSG_ID_MASK : 0;
    uint16_t node_type_mask = broadcast_group_active ? H9MSG_NODE_TYPE_MASK : 0;

    set_CAN_broadcast_id(0, H9MSG_ALL_BROADCAST_MSG_TYPE_GROUP, 0, remote_node_id, broadcast_group);
    set_CAN_broadcast_id_mask(0, H9MSG_ALL_BROADCAST_MSG_TYPE_GROUP_MASK, 0, node_id_mask, node_type_mask);

    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    CANIE2 |= 1 << IEMOB4;
}


uint8_t CAN_try_put_msg(h9msg_t *cm) {
    CANPAGE = 0 << MOBNB0;              // Select MOb0 for transmission

    if (CANEN2 & ( 1 << ENMOB0 )) {
        return 0;
    }

    CANSTMOB = 0x00;                    // Clear mob status register

    set_CAN_id(cm);

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
            calc_can_id(&can_tx_buf[can_tx_buf_top].canidt1, &can_tx_buf[can_tx_buf_top].canidt2, &can_tx_buf[can_tx_buf_top].canidt3, &can_tx_buf[can_tx_buf_top].canidt4, cm);

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
        cm->flags = ((can_rx_buf[can_rx_buf_bottom].canidt1) & 0x03);
        cm->source_id = can_rx_buf[can_rx_buf_bottom].canidt2;
        cm->destination_id  = can_rx_buf[can_rx_buf_bottom].canidt3;
        cm->seqnum = (can_rx_buf[can_rx_buf_bottom].canidt4 >> 3) & 0x1f;

        cm->dlc = can_rx_buf[can_rx_buf_bottom].cancdmob & 0x0f;
        uint8_t idx = 0;
        for (; idx < 8; ++idx)
            cm->data[idx] = can_rx_buf[can_rx_buf_bottom].data[idx];

        can_rx_buf_bottom = (uint8_t)((can_rx_buf_bottom + 1) & CAN_RX_BUF_INDEX_MASK);

        // 1st msg filter: mob filter/mask
        // 2nd msg filter
        // if (cm->source_id == H9MSG_BROADCAST_ID) { //invalid message, drop
        //     return 0;
        // }

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
        case H9MSG_TYPE_SET_REG:
        case H9MSG_TYPE_SET_BIT:
        case H9MSG_TYPE_CLEAR_BIT:
            res->type = H9MSG_TYPE_REG_VALUE;
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
    can_node_id = eeprom_read_byte(&ee_node_id);
    // if (node_id > 0 && node_id < 0xff) {
    //     can_node_id = node_id;
    // }
    // else {
    //     can_node_id = 0;
    // }
}


void write_node_id(uint8_t id) {
    cli();
    eeprom_write_byte(&ee_node_id, id);
    sei();
}


static void calc_can_id(volatile uint8_t* id1, volatile uint8_t* id2, volatile uint8_t* id3, volatile uint8_t* id4, h9msg_t* cm) {
    calc_can_unicast_id(id1, id2, id3, id4, cm->priority, cm->type, cm->flags, cm->source_id, cm->destination_id, cm->seqnum);
}


static void calc_can_unicast_id(volatile uint8_t* id1, volatile uint8_t* id2, volatile uint8_t* id3, volatile uint8_t* id4, uint8_t priority, uint8_t type, uint8_t flags, uint8_t src, uint8_t dst, uint8_t seq) {
    *id1 = ((priority << 7) & 0x80) | ((type << 2) & 0x7c) | (flags & 0x03);
    *id2 = src;
    *id3 = dst;
    *id4 = ((seq << 3) & 0xf8);
}


static void calc_can_broadcast_id(volatile uint8_t* id1, volatile uint8_t* id2, volatile uint8_t* id3, volatile uint8_t* id4, uint8_t priority, uint8_t type, uint8_t flags, uint8_t src, uint16_t node_type) {
    calc_can_unicast_id(id1, id2, id3, id4, priority, type, flags, src, (node_type >> 5) & 0xff, ((node_type << 3) & 0xf8));
}


static void set_CAN_id(h9msg_t* cm) {
    calc_can_id(&CANIDT1, &CANIDT1, &CANIDT2, &CANIDT3, cm);
}


static void set_CAN_unicast_id(uint8_t priority, uint8_t type, uint8_t flags, uint8_t src, uint8_t dst, uint8_t seq) {
    calc_can_unicast_id(&CANIDT1, &CANIDT1, &CANIDT2, &CANIDT3, priority, type & 0x0f, flags, src, dst, seq);
}

static void set_CAN_unicast_id_mask(uint8_t priority, uint8_t type, uint8_t flags, uint8_t src, uint8_t dst, uint8_t seq) {
    calc_can_unicast_id(&CANIDM1, &CANIDM2, &CANIDM3, &CANIDM4, priority, type | 0x10, flags, src, dst, seq);
}


static void set_CAN_broadcast_id(uint8_t priority, uint8_t type, uint8_t flags, uint8_t src, uint16_t node_type) {
    calc_can_broadcast_id(&CANIDT1, &CANIDT1, &CANIDT2, &CANIDT3, priority, type | 0x10, 0, src, node_type);
}


static void set_CAN_broadcast_id_mask(uint8_t priority, uint8_t type, uint8_t flags, uint8_t src, uint16_t node_type) {
    calc_can_broadcast_id(&CANIDM1, &CANIDM2, &CANIDM3, &CANIDM4, priority, type | 0x10, 3, src, node_type);
}
