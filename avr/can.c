// SPDX-License-Identifier: MIT
/*
 * H9 CAN protocol implementation for AVR
 *
 * Copyright (C) 2017-2026 Kamil Pałkowski
 *
 */

#include <string.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/boot.h>

#include <h9def.h>

#include "h9avr/can.h"
#include "h9avr/node_id.h"
#include "h9avr/bl_info.h"

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
static volatile uint8_t can_rx_buf_overflow = 0;

static can_buf_t can_tx_buf[CAN_TX_BUF_SIZE];
static volatile uint8_t can_tx_buf_top = 0;
static volatile uint8_t can_tx_buf_bottom = 0;

volatile uint8_t can_node_id;
#ifdef __AVR__
static uint8_t reset_reason __attribute__ ((section (".noinit")));
#else
static uint8_t reset_reason;
#endif

static struct {
    uint16_t node_type;
    uint8_t pcb_revision;
    uint8_t bom_revision;
    uint16_t version_major;
    uint16_t version_minor;
    uint16_t version_patch;
    char build_info[H9FRAME_MAX_REGISTER_SIZE];
    volatile union {                        // NODE_FLAG_* in h9def.h, register 0, NODE_INFO data[6..7]
        struct {
            unsigned reset_reason : 3;      //0-2
            unsigned bl_present : 1;        //3
            unsigned bl_mismatch : 1;       //4
            unsigned default_id : 1;        //5
            unsigned can_error_warning : 1; //6, sticky
            unsigned can_tx_frame_loss : 1; //7, sticky
            unsigned can_rx_frame_loss : 1; //8, sticky
        };
        uint16_t raw;
    } flags;
} node_info;

/* --- Forward declarations --- */

#ifdef __AVR__
static void __attribute__((noreturn)) mcu_reset(void);
#else
void __attribute__((weak)) mcu_reset(void);
#endif
#ifdef __AVR__
static void set_mandatory_message_fields(h9frame_t *cm);
#else
void set_mandatory_message_fields(h9frame_t *cm);
#endif
static void calc_can_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, h9frame_t *cm);
static void calc_can_unicast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t src, uint8_t flags, uint8_t dst, uint8_t seq);
static void calc_can_broadcast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t src, uint16_t node_type);
static void set_CAN_id(h9frame_t *cm);
static void set_CAN_unicast_id(uint8_t type, uint8_t flags, uint8_t src, uint8_t dst, uint8_t seq);
static void set_CAN_unicast_id_mask(uint8_t type, uint8_t flags, uint8_t src, uint8_t dst, uint8_t seq);
static void set_CAN_broadcast_id(uint8_t type, uint8_t src, uint16_t node_type);
static void set_CAN_broadcast_id_mask(uint8_t type, uint8_t src, uint16_t node_type);
static void send_reg_value1(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value);
static void send_reg_value2(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2);
static void send_reg_value6(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3, uint8_t value4, uint8_t value5, uint8_t value6);
static void CAN_send_node_info_broadcast(uint8_t type);
static void process_standard_reg(h9frame_t *cm);
static uint8_t process_msg(h9frame_t *cm);

/* ======================== PUBLIC FUNCTIONS ======================== */

uint8_t CAN_init(uint16_t node_type, uint8_t default_id, uint8_t pcb_rev, uint8_t bom_rev, uint16_t version_major, uint16_t version_minor, uint16_t version_patch, const char *build_info) {
    uint8_t ret = 1;

    node_info.node_type = node_type;
    node_info.pcb_revision = pcb_rev;
    node_info.bom_revision = bom_rev;
    node_info.version_major = version_major;
    node_info.version_minor = version_minor;
    node_info.version_patch = version_patch;
    strncpy(node_info.build_info, build_info, H9FRAME_MAX_REGISTER_SIZE);

    node_id_repair();          // restore a damaged EEPROM copy of the node id (writes only if needed)
    can_node_id = read_node_id();
    if (can_node_id == 0) {    // no valid id in EEPROM
        can_node_id = default_id;
        ret = 0;
    }

    node_info.flags.raw = 0;
    node_info.flags.reset_reason = reset_reason;
    node_info.flags.default_id = !ret;

    h9_bl_info_t bl;
    if (read_bl_info(&bl)) {
        node_info.flags.bl_present = 1;
        node_info.flags.bl_mismatch = bl.node_type != node_type || bl.pcb_rev != pcb_rev || bl.bom_rev != bom_rev;
    }

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
    set_CAN_unicast_id(H9FRAME_UNICAST_MSG_TYPE_GROUP, 0, 0, can_node_id, 0);
    set_CAN_unicast_id_mask(H9FRAME_UNICAST_MSG_TYPE_GROUP_MASK, 0, 0, H9FRAME_ID_MASK, 0);
    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    //select mob 2 for broadcast
    CANPAGE = 0x02 << MOBNB0;
    set_CAN_broadcast_id(H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP, 0, H9FRAME_BROADCAST_ALL_GROUP);
    set_CAN_broadcast_id_mask(H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP_MASK, 0, H9FRAME_NODE_TYPE_MASK);
    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    CANPAGE = 0x03 << MOBNB0;
    set_CAN_broadcast_id(H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP, 0, node_type);
    set_CAN_broadcast_id_mask(H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP_MASK, 0, H9FRAME_NODE_TYPE_MASK);
    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    CANIE2 = ( 1 << IEMOB0 ) | ( 1 << IEMOB1 ) | ( 1 << IEMOB2 ) | ( 1 << IEMOB3 ); //interupt mob 0 1 2 3

    CANGIE = (1<<ENBOFF) | (1<<ENIT) | (1<<ENRX) | (1<<ENTX) | (1<<ENERR) | (1<<ENBX) | (1<<ENERG);
    CANGCON = 1<<ENASTB;

    return ret;
}

// Error counter reached the warning limit (96), like TXWARN/RXWARN on PIC.
// CANTEC is 8-bit and can't show bus off (TEC > 255), hence the CANGSTA check.
uint8_t CAN_bus_error_warning(void) {
    return CANTEC >= 96 || CANREC >= 96 || (CANGSTA & ((1 << ERRP) | (1 << BOFF)));
}

void CAN_send_turned_on_broadcast(void) {
    CAN_send_node_info_broadcast(H9FRAME_TYPE_NODE_TURNED_ON);
}

void CAN_set_msg_filter_1(uint16_t broadcast_group) {
    CANPAGE = 0x04 << MOBNB0; //select mob 4

    set_CAN_broadcast_id(H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP, 0, broadcast_group);
    set_CAN_broadcast_id_mask(H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP_MASK, 0, H9FRAME_NODE_TYPE_MASK);

    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    CANIE2 |= 1 << IEMOB4;
}

void CAN_set_msg_filter_2(uint16_t broadcast_group) {
    CANPAGE = 0x05 << MOBNB0; //select mob 5

    set_CAN_broadcast_id(H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP, 0, broadcast_group);
    set_CAN_broadcast_id_mask(H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP_MASK, 0, H9FRAME_NODE_TYPE_MASK);

    CANIDM4 |= 1 << IDEMSK; // set filter
    CANCDMOB = (1<<CONMOB1) | (1<<IDE); //rx mob, 29-bit only

    CANIE2 |= 1 << IEMOB5;
}

uint8_t CAN_try_put_msg(h9frame_t *cm) {
    cm->source_id = can_node_id;

    if (cm->type & H9FRAME_UNICAST_BROADCAST_BIT)
        cm->broadcast.group = node_info.node_type;

    uint8_t sreg = SREG;
    cli();
    CANPAGE = 0 << MOBNB0;              // Select MOb0 for transmission

    if (CANEN2 & ( 1 << ENMOB0 )) {
        SREG = sreg;
        return 0;
    }

    CANSTMOB = 0x00;                    // Clear mob status register

    set_CAN_id(cm);

    for (uint8_t idx = 0; idx < cm->dlc; ++idx) {
        CANMSG = cm->data[idx];
    }

    CANCDMOB = (1 << CONMOB0) | (1 << IDE) | (cm->dlc & 0x0f);
    SREG = sreg;
    return 1;
}

uint8_t __attribute__((weak)) CAN_put_msg(h9frame_t *cm) {
    uint8_t sreg = SREG;
    cli();
    uint8_t ret = 0;
    if (CAN_try_put_msg(cm)) {
        ret = 1;
    }
    else {
        uint8_t tmp_idx = (uint8_t) ((can_tx_buf_top + 1) & CAN_TX_BUF_INDEX_MASK);

        //TODO: rozwarzyc czy nie zwiekszyc bo przy multi frame sie ten bufor skonczy, obecnie jest 8, do 64 na chwile obecna da sie powiekszyc, moze dodac wysylanie blokujace?
        // ale trzeba wziasc pod uwage ze nie mozna czekac z wylaczonymi przerwaniami
        if (can_tx_buf_bottom != tmp_idx) {
            calc_can_id(&can_tx_buf[can_tx_buf_top].canidt1, &can_tx_buf[can_tx_buf_top].canidt2, &can_tx_buf[can_tx_buf_top].canidt3, &can_tx_buf[can_tx_buf_top].canidt4, cm);

            for (uint8_t idx = 0; idx < cm->dlc; ++idx)
                can_tx_buf[can_tx_buf_top].data[idx] = cm->data[idx];

            can_tx_buf[can_tx_buf_top].cancdmob = cm->dlc & 0x0f;

            can_tx_buf_top = tmp_idx;
            ret = 2;
        }
        else if (CANGSTA & ((1 << ERRP) | (1 << BOFF))) {    //bus passive / bus off error
            // replace the newest queued frame with NODE_FAULT, it will be sent once the bus recovers
            uint8_t last_idx = (uint8_t) ((can_tx_buf_top - 1) & CAN_TX_BUF_INDEX_MASK);

            h9frame_t fault;
            fault.type = H9FRAME_TYPE_NODE_FAULT;
            fault.source_id = can_node_id;
            fault.broadcast.group = node_info.node_type;
            calc_can_id(&can_tx_buf[last_idx].canidt1, &can_tx_buf[last_idx].canidt2, &can_tx_buf[last_idx].canidt3, &can_tx_buf[last_idx].canidt4, &fault);

            can_tx_buf[last_idx].data[0] = NODE_FAULT_CAN_FRAME_LOSS;
            can_tx_buf[last_idx].cancdmob = 1;
            node_info.flags.can_tx_frame_loss = 1;  // interrupts disabled here
        }
    }
    SREG = sreg;
    return ret;
}

void send_command_error(uint8_t errno, uint8_t destination, uint8_t seqnum) {
    h9frame_t cm;
    cm.type = H9FRAME_TYPE_COMMAND_ERROR;
    cm.unicast.flags = 0;
    cm.unicast.destination_id = destination;
    cm.unicast.seqnum = seqnum;

    cm.data[0] = errno;
    cm.dlc = 1;
    CAN_put_msg(&cm);
}

void send_node_fault(uint8_t errno) {
    h9frame_t cm;
    cm.type = H9FRAME_TYPE_NODE_FAULT;

    cm.data[0] = errno;
    cm.dlc = 1;
    CAN_put_msg(&cm);
}

// 31 30 29 | 28     27 26 25 24 23 22 21 | 20 19 18 17 16 15 14 13 | 12 11 10 09 08 07 06 05 | 04 03 02 01 00
// -- -- -- | ty_(0) ty ty ty ty so so so | so so so so so fl fl fl | ds ds ds ds ds ds ds ds | sq sq sq sq sq
// -- -- -- | ty_(1) ty ty ty ty so so so | so so so so so nt nt nt | nt nt nt nt nt nt nt nt | nt nt nt nt nt
uint8_t CAN_get_msg(h9frame_t *cm) {
    if (CAN_bus_error_warning()) {
        uint8_t sreg = SREG;    // flags are also set from interrupts (CAN_put_msg), keep the read-modify-write atomic
        cli();
        node_info.flags.can_error_warning = 1;
        SREG = sreg;
    }

    // RX frame lost: software buffer full
    if (can_rx_buf_overflow) {
        can_rx_buf_overflow = 0;
        uint8_t sreg = SREG;
        cli();
        node_info.flags.can_rx_frame_loss = 1;
        SREG = sreg;
        send_node_fault(NODE_FAULT_CAN_RX_FRAME_LOSS);
    }

    if (can_rx_buf_top != can_rx_buf_bottom) {
        cm->type = can_rx_buf[can_rx_buf_bottom].canidt1 >> 3;
        cm->source_id = (can_rx_buf[can_rx_buf_bottom].canidt1 << 5) | (can_rx_buf[can_rx_buf_bottom].canidt2 >> 3);
        if (cm->type & H9FRAME_UNICAST_BROADCAST_BIT) {
            cm->broadcast.group = ((uint16_t)can_rx_buf[can_rx_buf_bottom].canidt2 << 13) | ((uint16_t)can_rx_buf[can_rx_buf_bottom].canidt3 << 5) | ((can_rx_buf[can_rx_buf_bottom].canidt4 >> 3) & 0x1f);
        }
        else {
            cm->unicast.flags = ((can_rx_buf[can_rx_buf_bottom].canidt2) & 0x07);
            cm->unicast.destination_id  = can_rx_buf[can_rx_buf_bottom].canidt3;
            cm->unicast.seqnum = can_rx_buf[can_rx_buf_bottom].canidt4 >> 3;
        }
        cm->dlc = can_rx_buf[can_rx_buf_bottom].cancdmob & 0x0f;
        uint8_t idx = 0;
        for (; idx < 8; ++idx)
            cm->data[idx] = can_rx_buf[can_rx_buf_bottom].data[idx];

        can_rx_buf_bottom = (uint8_t)((can_rx_buf_bottom + 1) & CAN_RX_BUF_INDEX_MASK);

        return process_msg(cm);
    }
    return 0;
}

/* ======================== STATIC FUNCTIONS ======================== */

/* Runs before main() in .init3 to capture and clear MCUSR before watchdog disable */
#ifdef __AVR__
__attribute__((naked)) __attribute__((section(".init3"))) void wdt_init(void) {
    if (MCUSR == ((1 << PORF) | (1 << BORF))) reset_reason = NODE_RESET_BY_POWER_ON;
    else if (MCUSR == (1 << WDRF)) reset_reason = NODE_RESET_BY_WATCHDOG;
    else if (MCUSR == (1 << BORF)) reset_reason = NODE_RESET_BY_BROWN_OUT;
    else if (MCUSR == (1 << EXTRF)) reset_reason = NODE_RESET_BY_EXTERNAL_SOURCE;
    else reset_reason = NODE_RESET_BY_UNKNOWN;

    MCUSR = 0;
    wdt_disable();
    return;
}
#endif /* __AVR__ */

#ifdef __AVR__
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
            uint8_t next_top = (uint8_t)((can_rx_buf_top + 1) & CAN_RX_BUF_INDEX_MASK);
            if (next_top == can_rx_buf_bottom) {
                can_rx_buf_overflow = 1;
            }
            else {
                can_rx_buf[can_rx_buf_top].canidt1 = CANIDT1;
                can_rx_buf[can_rx_buf_top].canidt2 = CANIDT2;
                can_rx_buf[can_rx_buf_top].canidt3 = CANIDT3;
                can_rx_buf[can_rx_buf_top].canidt4 = CANIDT4;
                can_rx_buf[can_rx_buf_top].cancdmob = CANCDMOB & 0x1f;
                for (uint8_t i = 0; i < 8; ++i) {
                    can_rx_buf[can_rx_buf_top].data[i] = CANMSG;
                }
                can_rx_buf_top = next_top;
            }
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
#endif /* __AVR__ */

#ifdef __AVR__
static void __attribute__((noreturn)) mcu_reset(void) {
#else
void __attribute__((weak)) mcu_reset(void) {
#endif
    cli();
    do {
        wdt_enable(WDTO_15MS);
        for(;;) {
        }
    } while(0);
}

static void calc_can_unicast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t src, uint8_t flags, uint8_t dst, uint8_t seq) {
    *id1 = (type << 3) | (src >> 5);
    *id2 = (src << 3) | (flags & 0x07);
    *id3 = dst;
    *id4 = ((seq << 3) & 0xf8);
}

static void calc_can_broadcast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t src, uint16_t node_type) {
    *id1 = (type << 3) | (src >> 5);
    *id2 = (src << 3) | ((node_type >> 13) & 0x07);
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

static void set_CAN_broadcast_id(uint8_t type, uint8_t src, uint16_t node_type) {
    calc_can_broadcast_id(&CANIDT1, &CANIDT2, &CANIDT3, &CANIDT4, type | 0x10, src, node_type);
}

static void set_CAN_broadcast_id_mask(uint8_t type, uint8_t src, uint16_t node_type) {
    calc_can_broadcast_id(&CANIDM1, &CANIDM2, &CANIDM3, &CANIDM4, type | 0x10, src, node_type);
}

void CAN_send_reg_value(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t *value, size_t length) {
    h9frame_t cm;
    cm.type = H9FRAME_TYPE_REG_VALUE;
    cm.unicast.destination_id = destination;
    cm.unicast.seqnum = seqnum;

    size_t value_ix = 0;

    for (uint8_t msg_num = 0; value_ix < length; ++msg_num) {
        uint8_t i = 1;
        cm.data[0] = registry;
        
        if (length < 8) {
            cm.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;
        }
        else if (msg_num == 0) {
            cm.unicast.flags = H9FRAME_FLAG_MULTI_FRAME_FIRST;
            cm.data[1] = (uint8_t)((length + 5) / 6);
            i++;
        }
        else if (value_ix < length - 6) {
            cm.unicast.flags = H9FRAME_FLAG_MULTI_FRAME_MIDDLE;
            cm.data[1] = msg_num;
            i++;
        }
        else {
            cm.unicast.flags = H9FRAME_FLAG_MULTI_FRAME_LAST;
            cm.data[1] = msg_num;
            i++;
        }
        
        for (; i < 8 && value_ix < length; ++i) {
            cm.data[i] = value[value_ix];
            value_ix++;
        }
        cm.dlc = i;

        CAN_put_msg(&cm);
    }
}

static void send_reg_value1(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value) {
    h9frame_t cm;
    cm.type = H9FRAME_TYPE_REG_VALUE;
    cm.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;
    cm.unicast.destination_id = destination;
    cm.unicast.seqnum = seqnum;

    cm.data[0] = registry;
    cm.data[1] = value;
    cm.dlc = 2;
    CAN_put_msg(&cm);
}

static void send_reg_value2(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2) {
    h9frame_t cm;

    cm.type = H9FRAME_TYPE_REG_VALUE;
    cm.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;
    cm.unicast.destination_id = destination;
    cm.unicast.seqnum = seqnum;

    cm.data[0] = registry;
    cm.data[1] = value1;
    cm.data[2] = value2;
    cm.dlc = 3;
    CAN_put_msg(&cm);
}

static void send_reg_value6(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3, uint8_t value4, uint8_t value5, uint8_t value6) {
    h9frame_t cm;
    cm.type = H9FRAME_TYPE_REG_VALUE;
    cm.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;
    cm.unicast.destination_id = destination;
    cm.unicast.seqnum = seqnum;

    cm.data[0] = registry;
    cm.data[1] = value1;
    cm.data[2] = value2;
    cm.data[3] = value3;
    cm.data[4] = value4;
    cm.data[5] = value5;
    cm.data[6] = value6;
    cm.dlc = 7;
    CAN_put_msg(&cm);
}

// type: H9FRAME_TYPE_NODE_INFO or H9FRAME_TYPE_NODE_TURNED_ON
static void CAN_send_node_info_broadcast(uint8_t type) {
    h9frame_t cm;

    cm.type = type;

    // version packed into 32 bits: major (10 bits) | minor (11 bits) | patch (11 bits), same as BOOTLOADER_TURNED_ON
    uint32_t version = ((uint32_t)(node_info.version_major & 0x3ff) << 22) | ((uint32_t)(node_info.version_minor & 0x7ff) << 11) | (node_info.version_patch & 0x7ff);
    uint16_t flags = node_info.flags.raw;

    cm.dlc = 8;
    cm.data[0] = (version >> 24) & 0xff;
    cm.data[1] = (version >> 16) & 0xff;
    cm.data[2] = (version >> 8) & 0xff;
    cm.data[3] = version & 0xff;
    cm.data[4] = node_info.pcb_revision;
    cm.data[5] = node_info.bom_revision;
    cm.data[6] = (flags >> 8) & 0xff;
    cm.data[7] = flags & 0xff;
    CAN_put_msg(&cm);
}

void __attribute__((weak)) read_power_supply_register(uint8_t destination_id, uint8_t seqnum) {
    send_command_error(H9FRAME_ERROR_UNSUPPORTED_REGISTER, destination_id, seqnum);
}

void __attribute__((weak)) read_mcu_temp_register(uint8_t destination_id, uint8_t seqnum) {
    send_command_error(H9FRAME_ERROR_UNSUPPORTED_REGISTER, destination_id, seqnum);
}

static void process_standard_reg(h9frame_t *cm) {
    if (cm->type == H9FRAME_TYPE_SET_REG && cm->dlc > 1) {
        switch (cm->data[0]) {
            case NODE_FLAGS_STD_REGISTER:
            case NODE_TYPE_STD_REGISTER:
            case NODE_HARDWARE_REVISION_STD_REGISTER:
            case NODE_VERSION_STD_REGISTER:
            case NODE_BUILD_INFO_STD_REGISTER:
            case NODE_MCU_TYPE_STD_REGISTER:
            case NODE_SN_STD_REGISTER:
            case NODE_POWER_SUPPLY_STD_REGISTER:
            case NODE_MCU_TEMP_STD_REGISTER:
                send_command_error(H9FRAME_ERROR_READ_ONLY_REGISTER, cm->source_id, cm->unicast.seqnum);
                return;
            case NODE_ID_STD_REGISTER:
                if (cm->dlc == 2) {
                    if (cm->data[1] == 0 || cm->data[1] == 0xff) {     // 0 = no id (default id is used), 0xff reserved
                        send_command_error(H9FRAME_ERROR_INVALID_VALUE, cm->source_id, cm->unicast.seqnum);
                        return;
                    }
                    // takes ~25 ms (EEPROM writes); avr-libc disables interrupts only for the EEPE timed sequence
                    write_node_id(cm->data[1], node_info.node_type);

                    send_reg_value1(NODE_ID_STD_REGISTER, cm->source_id, cm->unicast.seqnum, can_node_id);
                    return;
                }
                else {
                    send_command_error(H9FRAME_ERROR_REGISTER_SIZE_MISMATCH, cm->source_id, cm->unicast.seqnum);
                    return;
                }
            default:
                send_command_error(H9FRAME_ERROR_UNSUPPORTED_REGISTER, cm->source_id, cm->unicast.seqnum);
                return;
        }
    }
    else if (cm->type == H9FRAME_TYPE_GET_REG && cm->dlc == 1) {
        switch (cm->data[0]) {
            case NODE_FLAGS_STD_REGISTER: {
                uint16_t flags = node_info.flags.raw;
                send_reg_value2(NODE_FLAGS_STD_REGISTER, cm->source_id, cm->unicast.seqnum, (flags >> 8) & 0xff, flags & 0xff);
                return;
            }
            case NODE_TYPE_STD_REGISTER:
                send_reg_value2(NODE_TYPE_STD_REGISTER, cm->source_id, cm->unicast.seqnum, (node_info.node_type >> 8) & 0xff, (node_info.node_type) & 0xff);
                return;
            case NODE_HARDWARE_REVISION_STD_REGISTER:
                send_reg_value2(NODE_HARDWARE_REVISION_STD_REGISTER, cm->source_id, cm->unicast.seqnum, node_info.pcb_revision, node_info.bom_revision);
                return;
            case NODE_VERSION_STD_REGISTER:
                send_reg_value6(NODE_VERSION_STD_REGISTER, cm->source_id, cm->unicast.seqnum, (node_info.version_major >> 8), node_info.version_major & 0xff, (node_info.version_minor >> 8) & 0xff, node_info.version_minor & 0xff, (node_info.version_patch >> 8) & 0xff, node_info.version_patch & 0xff);
                return;
            case NODE_BUILD_INFO_STD_REGISTER: {
                size_t len = 0;
                for (; node_info.build_info[len] && len < (H9FRAME_MAX_REGISTER_SIZE - 1); len++);
                CAN_send_reg_value(NODE_BUILD_INFO_STD_REGISTER, cm->source_id, cm->unicast.seqnum, (uint8_t*)&node_info.build_info, len);
                return;
            }
            case NODE_MCU_TYPE_STD_REGISTER:
#if defined (__AVR_ATmega16M1__)
                send_reg_value1(NODE_MCU_TYPE_STD_REGISTER, cm->source_id, cm->unicast.seqnum, NODE_MCU_ATMEGA16M1);
#elif defined (__AVR_ATmega32M1__)
                send_reg_value1(NODE_MCU_TYPE_STD_REGISTER, cm->source_id, cm->unicast.seqnum, NODE_MCU_ATMEGA32M1);
#elif defined (__AVR_ATmega64M1__)
                send_reg_value1(NODE_MCU_TYPE_STD_REGISTER, cm->source_id, cm->unicast.seqnum, NODE_MCU_ATMEGA64M1);
#elif defined (__AVR_AT90CAN128__)
                send_reg_value1(NODE_MCU_TYPE_STD_REGISTER, cm->source_id, cm->unicast.seqnum, NODE_MCU_AT90CAN128);
#elif defined (__AVR_ATmega32C1__)
                send_reg_value1(NODE_MCU_TYPE_STD_REGISTER, cm->source_id, cm->unicast.seqnum, NODE_MCU_ATMEGA32C1);
#else
#error Unsupported MCU
#endif
                return;
#ifdef SIGRD    // signature row readable from software (not on AT90CAN128)
            case NODE_SN_STD_REGISTER: {
                // factory serial number: signature row 0x000E-0x0017 (lot number, wafer number, X/Y coordinates)
                uint8_t sn[10];
                for (uint8_t i = 0; i < sizeof(sn); i++)
                    sn[i] = boot_signature_byte_get(0x000E + i);
                CAN_send_reg_value(NODE_SN_STD_REGISTER, cm->source_id, cm->unicast.seqnum, sn, sizeof(sn));
                return;
            }
#endif
            case NODE_POWER_SUPPLY_STD_REGISTER:
                read_power_supply_register(cm->source_id, cm->unicast.seqnum);
                return;
            case NODE_MCU_TEMP_STD_REGISTER:
                read_mcu_temp_register(cm->source_id, cm->unicast.seqnum);
                return;
            case NODE_ID_STD_REGISTER:
                send_reg_value1(NODE_ID_STD_REGISTER, cm->source_id, cm->unicast.seqnum, can_node_id);
                return;
            default:
                send_command_error(H9FRAME_ERROR_UNSUPPORTED_REGISTER, cm->source_id, cm->unicast.seqnum);
                return;
        }
    }
    //H9FRAME_TYPE_SET_BIT, H9FRAME_TYPE_CLEAR_BIT
    send_command_error(H9FRAME_ERROR_UNSUPPORTED_OPERATION, cm->source_id, cm->unicast.seqnum);
}

static uint8_t process_msg(h9frame_t *cm) {
    /* --- BROADCAST --- */
    if (cm->type & H9FRAME_UNICAST_BROADCAST_BIT) {
        if (cm->type == H9FRAME_TYPE_DISCOVER || cm->type == H9FRAME_TYPE_GROUP_RESET) {
            if (cm->broadcast.group == node_info.node_type || cm->broadcast.group == H9FRAME_BROADCAST_ALL_GROUP) {
                if (cm->type == H9FRAME_TYPE_DISCOVER) {
                    CAN_send_node_info_broadcast(H9FRAME_TYPE_NODE_INFO);
                    return 0;
                }
                else if (cm->type == H9FRAME_TYPE_GROUP_RESET) {
                    mcu_reset();
                    return 0;
                }
            }
            else {
                // INVALID_MSG but we don't answere on broadcast
                return 0;
            }
        }

        return 2;
    }
    /* --- UNICAST --- */
    else {
        if (cm->unicast.destination_id != can_node_id) {
            return 0; //not for me
        }

        /* -- RCV BOOTLOADER MSG -- */
        if (cm->type <= H9FRAME_TYPE_PAGE_FILL_BREAK) {
            send_command_error(H9FRAME_ERROR_INVALID_FRAME, cm->source_id, cm->unicast.seqnum);
            return 0;
        }

        /* -- MULTIPLE MSG -- */
        if (cm->unicast.flags != 0 && cm->type != H9FRAME_TYPE_SET_REG && cm->type != H9FRAME_TYPE_REG_VALUE) {
            send_command_error(H9FRAME_ERROR_INVALID_FRAME, cm->source_id, cm->unicast.seqnum);
            return 0;
        }

        if (cm->type == H9FRAME_TYPE_NODE_RESET) {
            mcu_reset();
            return 0;
        }
        else if (cm->type == H9FRAME_TYPE_NODE_UPGRADE && cm->dlc == 0) {
#ifdef BOOTSTART
            cli();
            __asm__ volatile ( "jmp " STR(BOOTSTART) );
#else
#warning "Node upgrade (bootloader) disable"
            send_command_error(H9FRAME_ERROR_BOOTLOADER_UNSUPPORTED, cm->source_id, cm->unicast.seqnum);
            return 0;
#endif //BOOTSTART
        }
        else if (cm->type == H9FRAME_TYPE_SET_REG || cm->type == H9FRAME_TYPE_GET_REG || cm->type == H9FRAME_TYPE_SET_BIT || cm->type == H9FRAME_TYPE_CLEAR_BIT) {
            /* --- STANDARD REG OPERATION -- */
            if (cm->dlc > 0 && cm->data[0] < 10) {
                process_standard_reg(cm);
                return 0;
            }
            else {
                return 1;
            }
        }
        return 1; //THEORETICALLY H9FRAME_TYPE_COMMAND_ERROR OR H9FRAME_TYPE_REG_VALUE
    }
}
