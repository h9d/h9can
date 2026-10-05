/*
 * h9pic-can v0.2
 *
 * Created by SQ8KFH on 2020-07-11.
 *
 * Copyright (C) 2020-2021 Kamil Palkowski. All rights reserved.
 */

#include <string.h>
#include "h9pic/can.h"
#include "h9pic/ee_mem.h"
#include "h9pic/bl_info.h"
#include "h9frame.h"
#include "h9def.h"

#include "h9pic/common.h"

#define CAN_RX_BUF_SIZE 16
#define CAN_RX_BUF_INDEX_MASK 0x0F

#define CAN_MODE_NORMAL 0x00
#define CAN_MODE_CONFIG 0x80


static struct {
    uint8_t node_id;
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


typedef struct {
    uint8_t txbSIDH;
    uint8_t txbSIDL;
    uint8_t txbEIDH;
    uint8_t txbEIDL;
    uint8_t txbDLC;
    uint8_t data[8];
} can_buf_t;

can_buf_t can_rx_buf[CAN_RX_BUF_SIZE];
volatile uint8_t can_rx_buf_top = 0;
volatile uint8_t can_rx_buf_bottom = 0;
static volatile uint8_t can_rx_buf_overflow = 0;

#define BOOTLOADER_ADDR 0xF600      // pic_bootloader code offset

void (*read_power_supply_register)(uint8_t, uint8_t) = NULL;

static void calc_can_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, h9frame_t *cm);
static void calc_can_unicast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t flags, uint8_t src, uint8_t dst, uint8_t seq);
static void calc_can_broadcast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t src, uint16_t node_type);
static void send_reg_value1(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value);
static void send_reg_value2(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2);
static void send_reg_value3(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3);
static void send_reg_value4(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3, uint8_t value4);
static void send_reg_value6(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3, uint8_t value4, uint8_t value5, uint8_t value6);
static void CAN_send_node_info_broadcast(uint8_t type);
static void process_standard_reg(h9frame_t *cm);
static uint8_t process_msg(h9frame_t *cm);
static void can_set_mode(uint8_t mode);


void can_interrupt(void) {
    if (PIE5bits.RXB0IE && PIR5bits.RXB0IF) {
        PIR5bits.RXB0IF = 0;
        uint8_t next_top = (uint8_t)((can_rx_buf_top + 1) & CAN_RX_BUF_INDEX_MASK);
        if (next_top == can_rx_buf_bottom) {
            can_rx_buf_overflow = 1;
        }
        else {
            can_rx_buf[can_rx_buf_top].txbSIDH = RXB0SIDH;
            can_rx_buf[can_rx_buf_top].txbSIDL = RXB0SIDL;
            can_rx_buf[can_rx_buf_top].txbEIDH = RXB0EIDH;
            can_rx_buf[can_rx_buf_top].txbEIDL = RXB0EIDL;
            can_rx_buf[can_rx_buf_top].txbDLC = RXB0DLC;
            can_rx_buf[can_rx_buf_top].data[0] = RXB0D0;
            can_rx_buf[can_rx_buf_top].data[1] = RXB0D1;
            can_rx_buf[can_rx_buf_top].data[2] = RXB0D2;
            can_rx_buf[can_rx_buf_top].data[3] = RXB0D3;
            can_rx_buf[can_rx_buf_top].data[4] = RXB0D4;
            can_rx_buf[can_rx_buf_top].data[5] = RXB0D5;
            can_rx_buf[can_rx_buf_top].data[6] = RXB0D6;
            can_rx_buf[can_rx_buf_top].data[7] = RXB0D7;

            can_rx_buf_top = next_top;
        }
        RXB0CONbits.RXFUL = 0;
    }
    if (PIE5bits.RXB1IE && PIR5bits.RXB1IF) {
        PIR5bits.RXB1IF = 0;
        uint8_t next_top = (uint8_t)((can_rx_buf_top + 1) & CAN_RX_BUF_INDEX_MASK);
        if (next_top == can_rx_buf_bottom) {
            can_rx_buf_overflow = 1;
        }
        else {
            can_rx_buf[can_rx_buf_top].txbSIDH = RXB1SIDH;
            can_rx_buf[can_rx_buf_top].txbSIDL = RXB1SIDL;
            can_rx_buf[can_rx_buf_top].txbEIDH = RXB1EIDH;
            can_rx_buf[can_rx_buf_top].txbEIDL = RXB1EIDL;
            can_rx_buf[can_rx_buf_top].txbDLC = RXB1DLC;
            can_rx_buf[can_rx_buf_top].data[0] = RXB1D0;
            can_rx_buf[can_rx_buf_top].data[1] = RXB1D1;
            can_rx_buf[can_rx_buf_top].data[2] = RXB1D2;
            can_rx_buf[can_rx_buf_top].data[3] = RXB1D3;
            can_rx_buf[can_rx_buf_top].data[4] = RXB1D4;
            can_rx_buf[can_rx_buf_top].data[5] = RXB1D5;
            can_rx_buf[can_rx_buf_top].data[6] = RXB1D6;
            can_rx_buf[can_rx_buf_top].data[7] = RXB1D7;

            can_rx_buf_top = next_top;
        }
        RXB1CONbits.RXFUL = 0;
    }
    
}

uint8_t CAN_init(uint16_t node_type, uint8_t default_id, uint8_t pcb_rev, uint8_t bom_rev, uint16_t version_major, uint16_t version_minor, uint16_t version_patch, const char *build_info) {
    uint8_t ret = 1;
    //TODO: mozna doddac STACK FULL kiedy braknie nam stosu i STACK UNDERFLOW kiedy to zepsulismy stos i return nie zadzialal
//    if (STKPTRbits.STKFUL) {
//        reset_reason = NODE_RESET_BY_STACK_OVERFLOW;
//        STKPTRbits.STKFUL = 0;  // wyczyść ręcznie
//    } else if (STKPTRbits.STKUNF) {
//        reset_reason = NODE_RESET_BY_STACK_UNDERFLOW;
//        STKPTRbits.STKUNF = 0;
//    }
    if (!RCONbits.POR && !RCONbits.BOR) {
        node_info.flags.reset_reason = NODE_RESET_BY_POWER_ON;    // POR=0, BOR=0 → power-on
    } else if (RCONbits.TO == 0) {
        node_info.flags.reset_reason = NODE_RESET_BY_WATCHDOG;    // TO=0 → WDT timeout
    } else if (!RCONbits.BOR) {
        node_info.flags.reset_reason = NODE_RESET_BY_BROWN_OUT;   // BOR=0 → brown-out
    } else if (!RCONbits.RI) {
        node_info.flags.reset_reason = NODE_RESET_BY_SOFTWARE;    // RI=0 → RESET instr.
    //} else if (!RCONbits.RMCLR) {
    //    node_info.flags.reset_reason = NODE_RESET_BY_EXTERNAL_SOURCE; // MCLR pin
    } else {
        node_info.flags.reset_reason = NODE_RESET_BY_UNKNOWN;
    }
        
    //reset przyczyny resetu:D
    RCONbits.POR = 1;
    RCONbits.BOR = 1;
    RCONbits.RI  = 1;
    
    node_info.node_type = node_type;
    node_info.pcb_revision = pcb_rev;
    node_info.bom_revision = bom_rev;
    node_info.version_major = version_major;
    node_info.version_minor = version_minor;
    node_info.version_patch = version_patch;
    strncpy(node_info.build_info, build_info, H9FRAME_MAX_REGISTER_SIZE);
    
    node_info.node_id = read_node_id_and_refresh();
    if (node_info.node_id == 0xff) {    // no valid id in EEPROM
        node_info.node_id = default_id;
        ret = 0;
    }

    node_info.flags.default_id = !ret;

    h9_bl_info_t bl;
    if (read_bl_info(&bl)) {
        node_info.flags.bl_present = 1;
        node_info.flags.bl_mismatch = bl.node_type != node_type || bl.pcb_rev != pcb_rev || bl.bom_rev != bom_rev;
    }
    TRISBbits.TRISB2 = 1; //CANTX ax output
    TRISBbits.TRISB3 = 1; //CANRX ax input
    
    can_set_mode(CAN_MODE_CONFIG);

    ECANCON = 0x00;

    CIOCON = 0x21; // ?? 1 clock source

    // Mode 0: RXM0 is shared by RXF0 - RXF1 (RXB0), RXM1 by RXF2 - RXF5 (RXB1).
    // Filter registers are undefined after reset and all six are always enabled, so every one must be set.

    //RXB0: unicast to this node (RXF1 duplicates RXF0)
    calc_can_unicast_id(&RXM0SIDH, &RXM0SIDL, &RXM0EIDH, &RXM0EIDL, H9FRAME_UNICAST_MSG_TYPE_GROUP_MASK, 0, 0, H9FRAME_ID_MASK, 0);
    calc_can_unicast_id(&RXF0SIDH, &RXF0SIDL, &RXF0EIDH, &RXF0EIDL, H9FRAME_UNICAST_MSG_TYPE_GROUP, 0, 0, node_info.node_id, 0);
    calc_can_unicast_id(&RXF1SIDH, &RXF1SIDL, &RXF1EIDH, &RXF1EIDL, H9FRAME_UNICAST_MSG_TYPE_GROUP, 0, 0, node_info.node_id, 0);

    //RXB1: all broadcast types (16-31) for a given group; DISCOVER/GROUP_RESET are picked out in process_msg
    //(RXF4, RXF5 are placeholders until CAN_set_msg_filter_1/2)
    calc_can_broadcast_id(&RXM1SIDH, &RXM1SIDL, &RXM1EIDH, &RXM1EIDL, H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP_MASK, 0, H9FRAME_NODE_TYPE_MASK);
    calc_can_broadcast_id(&RXF2SIDH, &RXF2SIDL, &RXF2EIDH, &RXF2EIDL, H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP, 0, H9FRAME_BROADCAST_ALL_GROUP);
    calc_can_broadcast_id(&RXF3SIDH, &RXF3SIDL, &RXF3EIDH, &RXF3EIDL, H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP, 0, node_type);
    calc_can_broadcast_id(&RXF4SIDH, &RXF4SIDL, &RXF4EIDH, &RXF4EIDL, H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP, 0, node_type);
    calc_can_broadcast_id(&RXF5SIDH, &RXF5SIDL, &RXF5EIDH, &RXF5EIDL, H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP, 0, node_type);

    /**
        Baud rate: 125kbps
        System frequency: 16000000
        ECAN clock frequency: 16000000
        Time quanta: 8
        Sample point: 1-1-4-2
        Sample point: 75%
	*/

    BRGCON1 = 0x07;
    BRGCON2 = 0x98;
    BRGCON3 = 0x01;

    PIR5bits.RXB0IF = 0;    //reset
    PIE5bits.RXB0IE = 1;    //enable
    IPR5bits.RXB0IP = 0;    //low priority

    PIR5bits.RXB1IF = 0;    //reset
    PIE5bits.RXB1IE = 1;    //enable
    IPR5bits.RXB1IP = 0;    //low priority
    
    RXB0CON = 0b01000000;
    RXB1CON = 0b01000000;

    can_set_mode(CAN_MODE_NORMAL);

    return ret;
}

uint8_t CAN_bus_error_warning(void) {
    return COMSTATbits.TXWARN | COMSTATbits.RXWARN;
}

void CAN_send_turned_on_broadcast(void) {
    CAN_send_node_info_broadcast(H9FRAME_TYPE_NODE_TURNED_ON);
}

// Filter registers are writable in Configuration mode only
void CAN_set_msg_filter_1(uint16_t broadcast_group) {
    can_set_mode(CAN_MODE_CONFIG);
    calc_can_broadcast_id(&RXF4SIDH, &RXF4SIDL, &RXF4EIDH, &RXF4EIDL, H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP, 0, broadcast_group);
    can_set_mode(CAN_MODE_NORMAL);
}

void CAN_set_msg_filter_2(uint16_t broadcast_group) {
    can_set_mode(CAN_MODE_CONFIG);
    calc_can_broadcast_id(&RXF5SIDH, &RXF5SIDL, &RXF5EIDH, &RXF5EIDL, H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP, 0, broadcast_group);
    can_set_mode(CAN_MODE_NORMAL);
}

uint8_t CAN_put_msg(h9frame_t *cm) {
    uint8_t tempEIDH = 0;
    uint8_t tempEIDL = 0;
    uint8_t tempSIDH = 0;
    uint8_t tempSIDL = 0;
    
    //TOTO: ujednolicic gdzie ma bys ustawiane source_id i broadcast.group
    cm->source_id = node_info.node_id;
    
    if (cm->type & H9FRAME_UNICAST_BROADCAST_BIT) {
        cm->broadcast.group = node_info.node_type;
        calc_can_broadcast_id(&tempSIDH, &tempSIDL, &tempEIDH, &tempEIDL, cm->type, cm->source_id, cm->broadcast.group);
    }
    else
        calc_can_unicast_id(&tempSIDH, &tempSIDL, &tempEIDH, &tempEIDL, cm->type, cm->source_id, cm->unicast.flags, cm->unicast.destination_id, cm->unicast.seqnum);

    uint8_t gieh = INTCONbits.GIEH;
    uint8_t giel = INTCONbits.GIEL;

    for (;;) {
        INTCONbits.GIEH = 0;
        INTCONbits.GIEL = 0;

        if (TXB0CONbits.TXREQ != 1) {
            TXB0EIDH = tempEIDH;
            TXB0EIDL = tempEIDL;
            TXB0SIDH = tempSIDH;
            TXB0SIDL = tempSIDL;
            TXB0DLC  = cm->dlc;
            TXB0D0   = cm->data[0];
            TXB0D1   = cm->data[1];
            TXB0D2   = cm->data[2];
            TXB0D3   = cm->data[3];
            TXB0D4   = cm->data[4];
            TXB0D5   = cm->data[5];
            TXB0D6   = cm->data[6];
            TXB0D7   = cm->data[7];
            TXB0CONbits.TXREQ = 1;
            break;
        }
        else if (TXB1CONbits.TXREQ != 1) {
            TXB1EIDH = tempEIDH;
            TXB1EIDL = tempEIDL;
            TXB1SIDH = tempSIDH;
            TXB1SIDL = tempSIDL;
            TXB1DLC  = cm->dlc;
            TXB1D0   = cm->data[0];
            TXB1D1   = cm->data[1];
            TXB1D2   = cm->data[2];
            TXB1D3   = cm->data[3];
            TXB1D4   = cm->data[4];
            TXB1D5   = cm->data[5];
            TXB1D6   = cm->data[6];
            TXB1D7   = cm->data[7];
            TXB1CONbits.TXREQ = 1;
            break;
        }
        else if (TXB2CONbits.TXREQ != 1) {
            TXB2EIDH = tempEIDH;
            TXB2EIDL = tempEIDL;
            TXB2SIDH = tempSIDH;
            TXB2SIDL = tempSIDL;
            TXB2DLC  = cm->dlc;
            TXB2D0   = cm->data[0];
            TXB2D1   = cm->data[1];
            TXB2D2   = cm->data[2];
            TXB2D3   = cm->data[3];
            TXB2D4   = cm->data[4];
            TXB2D5   = cm->data[5];
            TXB2D6   = cm->data[6];
            TXB2D7   = cm->data[7];
            TXB2CONbits.TXREQ = 1;
            break;
        }
        else if (COMSTATbits.RXBP == 1 || COMSTATbits.TXBP == 1 || COMSTATbits.TXBO == 1) {    //bus passive / bus off error
            TXB2CONbits.TXREQ = 0;
            while (TXB2CONbits.TXREQ);

            uint8_t faultSIDH, faultSIDL, faultEIDH, faultEIDL;
            calc_can_broadcast_id(&faultSIDH, &faultSIDL, &faultEIDH, &faultEIDL, H9FRAME_TYPE_NODE_FAULT, node_info.node_id, node_info.node_type);

            TXB2EIDH = faultEIDH;
            TXB2EIDL = faultEIDL;
            TXB2SIDH = faultSIDH;
            TXB2SIDL = faultSIDL;
            TXB2DLC  = 1;
            TXB2D0   = NODE_FAULT_CAN_FRAME_LOSS;
            TXB2CONbits.TXREQ = 1;      // ramka czeka w TXB2 i pojedzie automatycznie po wyjściu z bus-off
            node_info.flags.can_tx_frame_loss = 1;  // interrupts disabled here

            INTCONbits.GIEH = gieh;
            INTCONbits.GIEL = giel;
            return 0;
        }

        // all TX buffers busy: re-enable interrupts so RX keeps being serviced while waiting
        INTCONbits.GIEH = gieh;
        INTCONbits.GIEL = giel;
    }

    INTCONbits.GIEH = gieh;
    INTCONbits.GIEL = giel;

    return 1;
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

//          | SIDH                        | SIDL                    | EIDH                    | EIDL
// 31 30 29 | 28     27 26 25 24 23 22 21 | 20 19 18 ** ** ** 17 16 | 15 14 13 12 11 10 09 08 | 07 06 05 04 03 02 01 00
// -- -- -- | ty_(0) ty ty ty ty so so so | so so so **  1 ** so so | fl fl fl ds ds ds ds ds | ds ds ds sq sq sq sq sq
// -- -- -- | ty_(1) ty ty ty ty so so so | so so so **  1 ** so so | nt nt nt nt nt nt nt nt | nt nt nt nt nt nt nt nt
uint8_t CAN_get_msg(h9frame_t* cm) {
    if (CAN_bus_error_warning()) {
        uint8_t gieh = INTCONbits.GIEH;     // flags are also set from interrupts, keep the read-modify-write atomic
        INTCONbits.GIEH = 0;
        node_info.flags.can_error_warning = 1;
        INTCONbits.GIEH = gieh;
    }

    // RX frame lost: software buffer full or hardware RXB0/RXB1 overflow
    if (can_rx_buf_overflow || COMSTATbits.RXB0OVFL || COMSTATbits.RXB1OVFL) {
        can_rx_buf_overflow = 0;
        uint8_t gieh = INTCONbits.GIEH;
        INTCONbits.GIEH = 0;
        node_info.flags.can_rx_frame_loss = 1;
        INTCONbits.GIEH = gieh;
        COMSTATbits.RXB0OVFL = 0;
        COMSTATbits.RXB1OVFL = 0;
        send_node_fault(NODE_FAULT_CAN_RX_FRAME_LOSS);
    }

    if (can_rx_buf_top != can_rx_buf_bottom) {
        cm->type = can_rx_buf[can_rx_buf_bottom].txbSIDH >> 3;
        cm->source_id = (uint8_t)(can_rx_buf[can_rx_buf_bottom].txbSIDH << 5) | (uint8_t)(can_rx_buf[can_rx_buf_bottom].txbSIDL >> 3 & 0x1c) | (uint8_t)(can_rx_buf[can_rx_buf_bottom].txbSIDL & 0x03);
        
        if (cm->type & H9FRAME_UNICAST_BROADCAST_BIT)
            cm->broadcast.group = (uint16_t)(can_rx_buf[can_rx_buf_bottom].txbEIDH << 8) | (uint16_t)(can_rx_buf[can_rx_buf_bottom].txbEIDL);
        else {
            cm->unicast.flags = can_rx_buf[can_rx_buf_bottom].txbEIDH >> 5;
            cm->unicast.destination_id = (uint8_t)(can_rx_buf[can_rx_buf_bottom].txbEIDH << 3) | (uint8_t)(can_rx_buf[can_rx_buf_bottom].txbEIDL >> 5);
            cm->unicast.seqnum = can_rx_buf[can_rx_buf_bottom].txbEIDL & 0x1f;            
        }

        cm->dlc = can_rx_buf[can_rx_buf_bottom].txbDLC & 0x0f;
        uint8_t idx = 0;
        for (; idx < 8; ++idx)
            cm->data[idx] = can_rx_buf[can_rx_buf_bottom].data[idx];

        can_rx_buf_bottom = (uint8_t)((can_rx_buf_bottom + 1) & CAN_RX_BUF_INDEX_MASK);

        return process_msg(cm);
    }
     
    return 0;
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

static void send_reg_value3(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3) {
    h9frame_t cm;
    cm.type = H9FRAME_TYPE_REG_VALUE;
    cm.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;
    cm.unicast.destination_id = destination;
    cm.unicast.seqnum = seqnum;

    cm.data[0] = registry;
    cm.data[1] = value1;
    cm.data[2] = value2;
    cm.data[3] = value3;
    cm.dlc = 4;
    CAN_put_msg(&cm);
}

static void send_reg_value4(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3, uint8_t value4) {
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
    cm.dlc = 5;
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
    cm.data[0] = (uint8_t)(version >> 24);
    cm.data[1] = (uint8_t)(version >> 16);
    cm.data[2] = (uint8_t)(version >> 8);
    cm.data[3] = (uint8_t)version;
    cm.data[4] = node_info.pcb_revision;
    cm.data[5] = node_info.bom_revision;
    cm.data[6] = (uint8_t)(flags >> 8);
    cm.data[7] = (uint8_t)flags;
    CAN_put_msg(&cm);
}

static void can_set_mode(uint8_t mode) {
    CANCON = mode;
    while ((CANSTAT & 0xE0) != mode);
}

static uint32_t read_serial_numer(void) {
    //doc: doc/SN.md
    uint32_t sn = 0;
    
    // index 0-7 → adresy 0x200000-0x200007
    TBLPTRU = 0x20;          // górny bajt adresu
    TBLPTRH = 0x00;
    TBLPTRL = 1;            // 0x00-0x07

    for (int i = 0; i < 4; ++i) {
        sn <<= 8;
        asm("TBLRD*+"); // odczyt do TABLAT i inkremantacja
        sn |= TABLAT;
    }
    return sn;
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
            //case NODE_MCU_TEMP_STD_REGISTER,
                send_command_error(H9FRAME_ERROR_READ_ONLY_REGISTER, cm->source_id, cm->unicast.seqnum);
                return;
            case NODE_ID_STD_REGISTER:
                if (cm->dlc == 2) {
                    write_node_id(cm->data[1]);
                    send_reg_value1(NODE_ID_STD_REGISTER, cm->source_id, cm->unicast.seqnum, node_info.node_id);
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
                send_reg_value2(NODE_FLAGS_STD_REGISTER, cm->source_id, cm->unicast.seqnum, (uint8_t)(flags >> 8), (uint8_t)flags);
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
                send_reg_value1(NODE_MCU_TYPE_STD_REGISTER, cm->source_id, cm->unicast.seqnum, NODE_MCU_PIC18F46K80);
                return;
            case NODE_SN_STD_REGISTER: {
                uint32_t sn = read_serial_numer();
                send_reg_value4(NODE_SN_STD_REGISTER, cm->source_id, cm->unicast.seqnum, (uint8_t)(sn >> 24), (uint8_t)(sn >> 16), (uint8_t)(sn >> 8), (uint8_t)(sn));
                return;
            }
            case NODE_POWER_SUPPLY_STD_REGISTER:
                if (read_power_supply_register) {
                    read_power_supply_register(cm->source_id, cm->unicast.seqnum);
                }
                else {
                    send_command_error(H9FRAME_ERROR_UNSUPPORTED_REGISTER, cm->source_id, cm->unicast.seqnum);
                }
                return;
            //case NODE_MCU_TEMP_STD_REGISTER:
            //     return;
            case NODE_ID_STD_REGISTER:
                send_reg_value1(NODE_ID_STD_REGISTER, cm->source_id, cm->unicast.seqnum, node_info.node_id);
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
                    RESET();
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
        if (cm->unicast.destination_id != node_info.node_id) {
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
            RESET();
            return 0;
        }
        else if (cm->type == H9FRAME_TYPE_NODE_UPGRADE && cm->dlc == 0) {
            INTCONbits.PEIE = 0;
            INTCONbits.GIE = 0;
            STKPTR = 0x00;
            asm ("goto " ___mkstr(BOOTLOADER_ADDR));
            return 0;
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
