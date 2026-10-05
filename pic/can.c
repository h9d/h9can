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
#include "h9frame.h"
#include "h9def.h"

#include "h9pic/common.h"

#define CAN_RX_BUF_SIZE 16
#define CAN_RX_BUF_INDEX_MASK 0x0F


static struct {
    uint8_t node_id;
    uint16_t node_type;
    char hardware_revision;
    uint16_t version_major;
    uint16_t version_minor;
    uint16_t version_patch;
    char build_info[H9FRAME_MAX_REGISTER_SIZE];
    uint8_t reset_reason;
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

void (*read_power_supply_register)(uint8_t, uint8_t) = NULL;

static void calc_can_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, h9frame_t *cm);
static void calc_can_unicast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t flags, uint8_t src, uint8_t dst, uint8_t seq);
static void calc_can_broadcast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t src, uint16_t node_type);
static void send_reg_value(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t *value, size_t length);
static void send_reg_value1(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value);
static void send_reg_value2(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2);
static void send_reg_value3(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3);
static void send_reg_value4(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3, uint8_t value4);
static void send_reg_value6(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3, uint8_t value4, uint8_t value5, uint8_t value6);
static void CAN_send_node_info_broadcast(uint8_t turn_on);
static void process_standard_reg(h9frame_t *cm);
static uint8_t process_msg(h9frame_t *cm);
static uint8_t read_hardware_revision(void);


void can_interrupt(void) {
    if (PIE5bits.RXB0IE && PIR5bits.RXB0IF) {
        PIR5bits.RXB0IF = 0;
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

        can_rx_buf_top = (uint8_t)((can_rx_buf_top + 1) & CAN_RX_BUF_INDEX_MASK);
        RXB0CONbits.RXFUL = 0;
    }
    if (PIE5bits.RXB1IE && PIR5bits.RXB1IF) {
        PIR5bits.RXB1IF = 0;
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

        can_rx_buf_top = (uint8_t)((can_rx_buf_top + 1) & CAN_RX_BUF_INDEX_MASK);
        RXB1CONbits.RXFUL = 0;
    }
    
}

void CAN_init(uint16_t node_type, uint8_t default_id, uint16_t version_major, uint16_t version_minor, uint16_t version_patch, const char *build_info) {
//TODO: mozna doddac STACK FULL kiedy braknie nam stosu i STACK UNDERFLOW kiedy to zepsulismy stos i return nie zadzialal
//    if (STKPTRbits.STKFUL) {
//        reset_reason = NODE_RESET_BY_STACK_OVERFLOW;
//        STKPTRbits.STKFUL = 0;  // wyczyść ręcznie
//    } else if (STKPTRbits.STKUNF) {
//        reset_reason = NODE_RESET_BY_STACK_UNDERFLOW;
//        STKPTRbits.STKUNF = 0;
//    }
    if (!RCONbits.POR && !RCONbits.BOR) {
        node_info.reset_reason = NODE_RESET_BY_POWER_ON;    // POR=0, BOR=0 → power-on
    } else if (RCONbits.TO == 0) {
        node_info.reset_reason = NODE_RESET_BY_WATCHDOG;    // TO=0 → WDT timeout
    } else if (!RCONbits.BOR) {
        node_info.reset_reason = NODE_RESET_BY_BROWN_OUT;   // BOR=0 → brown-out
    } else if (!RCONbits.RI) {
        node_info.reset_reason = NODE_RESET_BY_SOFTWARE;    // RI=0 → RESET instr.
    //} else if (!RCONbits.RMCLR) {
    //    node_info.reset_reason = NODE_RESET_BY_EXTERNAL_SOURCE; // MCLR pin
    } else {
        node_info.reset_reason = NODE_RESET_BY_UNKNOWN;
    }
        
    //reset przyczyny resetu:D
    RCONbits.POR = 1;
    RCONbits.BOR = 1;
    RCONbits.RI  = 1;
    
    node_info.node_type = node_type;
    node_info.hardware_revision = read_hardware_revision();
    node_info.version_major = version_major;
    node_info.version_minor = version_minor;
    node_info.version_patch = version_patch;
    strncpy(node_info.build_info, build_info, H9FRAME_MAX_REGISTER_SIZE);
    
    node_info.node_id = read_node_id_and_refresh();
    if (node_info.node_id == 0xff) {
        node_info.node_id = default_id;
    }
    TRISBbits.TRISB2 = 1; //CANTX ax output
    TRISBbits.TRISB3 = 1; //CANRX ax input
    
    CANCON = 0b10000000;
    while (0x80 != (CANSTAT & 0xE0));

    ECANCON = 0x00;

    CIOCON = 0x21; // ?? 1 clock source

    //mask for RXF0
    calc_can_unicast_id(&RXM0SIDH, &RXM0SIDL, &RXM0EIDH, &RXM0EIDL, H9FRAME_UNICAST_MSG_TYPE_GROUP_MASK, 0, 0, H9FRAME_ID_MASK, 0);
    calc_can_unicast_id(&RXF0SIDH, &RXF0SIDL, &RXF0EIDH, &RXF0EIDL, H9FRAME_UNICAST_MSG_TYPE_GROUP, 0, 0, node_info.node_id, 0);

    //mask for RXF1 - RXF5
    calc_can_broadcast_id(&RXM1SIDH, &RXM1SIDL, &RXM1EIDH, &RXM1EIDL, H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP_MASK, 0, H9FRAME_NODE_TYPE_MASK);
    calc_can_broadcast_id(&RXF1SIDH, &RXF1SIDL, &RXF1EIDH, &RXF1EIDL, H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP, 0, H9FRAME_BROADCAST_ALL_GROUP);
    calc_can_broadcast_id(&RXF2SIDH, &RXF2SIDL, &RXF2EIDH, &RXF2EIDL, H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP, 0, node_type);

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
    
    CANCON = 0;

    RXB0CON = 0b01000000;
    RXB1CON = 0b01000000;

    while (CANSTATbits.OPMODE0);
}

void CAN_send_turned_on_broadcast(void) {
    CAN_send_node_info_broadcast(1);
}

void CAN_set_msg_filter_1(uint16_t broadcast_group) {
    calc_can_broadcast_id(&RXF3SIDH, &RXF3SIDL, &RXF3EIDH, &RXF3EIDL, H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP, 0, broadcast_group);
}

void CAN_set_msg_filter_2(uint16_t broadcast_group) {
    calc_can_broadcast_id(&RXF4SIDH, &RXF4SIDL, &RXF4EIDH, &RXF4EIDL, H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP, 0, broadcast_group);
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

    INTCONbits.GIEH = 0;
    INTCONbits.GIEL = 0;

    while (TXB0CONbits.TXREQ == 1 && TXB1CONbits.TXREQ == 1 && TXB2CONbits.TXREQ == 1); //TODO: przerobic zeby przy zajetych kolejkach dodal do buforu wysylania
    
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

void CAN_init_new_msg(h9frame_t *fr) {
    fr->source_id = node_info.node_id;
    fr->unicast.flags = 0;
    fr->unicast.destination_id = 0;
    fr->unicast.seqnum = 0;
    fr->broadcast.group = 0;
    fr->dlc = 0;
}

void CAN_init_response_msg(const h9frame_t *req, h9frame_t *res) {
    res->unicast.seqnum = req->unicast.seqnum;
    switch (req->type) {
        case H9FRAME_TYPE_GET_REG:
        case H9FRAME_TYPE_SET_REG:
        case H9FRAME_TYPE_SET_BIT:
        case H9FRAME_TYPE_CLEAR_BIT:
            res->type = H9FRAME_TYPE_REG_VALUE;
            break;
        case H9FRAME_TYPE_DISCOVER:
            res->type = H9FRAME_TYPE_NODE_INFO;
            break;
        default:
            break;
    }
    res->source_id = node_info.node_id;
    res->unicast.destination_id = req->source_id;
    res->dlc = 0;
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

static void CAN_send_node_info_broadcast(uint8_t turn_on) {
    h9frame_t cm;

    if (turn_on)
        cm.type = H9FRAME_TYPE_NODE_TURNED_ON;
    else
        cm.type = H9FRAME_TYPE_NODE_INFO;
    cm.source_id = node_info.node_id;
    cm.broadcast.group = node_info.node_type;

    cm.dlc = 8;
    cm.data[0] = (node_info.node_type >> 8) & 0xff;
    cm.data[1] = (node_info.node_type) & 0xff;
    cm.data[2] = (node_info.version_major >> 8);
    cm.data[3] = node_info.version_major & 0xff;
    cm.data[4] = (node_info.version_minor >> 8) & 0xff;
    cm.data[5] = node_info.version_minor & 0xff;
    cm.data[6] = node_info.hardware_revision;
    cm.data[7] = node_info.reset_reason;
    CAN_put_msg(&cm);
}

static uint8_t read_hardware_revision(void) {
    // index 0-7 → adresy 0x200000-0x200007
    TBLPTRU = 0x20;          // górny bajt adresu
    TBLPTRH = 0x00;
    TBLPTRL = 0;            // 0x00-0x07

    return TABLAT;
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
            case NODE_TYPE_STD_REGISTER:
            case NODE_HARDWARE_REVISION_STD_REGISTER:
            case NODE_VERSION_STD_REGISTER:
            case NODE_BUILD_INFO_STD_REGISTER:
            case NODE_MCU_TYPE_STD_REGISTER:
            case NODE_SN_STD_REGISTER:
            case NODE_RESET_REASON_STD_REGISTER:
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
            case NODE_TYPE_STD_REGISTER:
                send_reg_value2(NODE_TYPE_STD_REGISTER, cm->source_id, cm->unicast.seqnum, (node_info.node_type >> 8) & 0xff, (node_info.node_type) & 0xff);
                return;
            case NODE_HARDWARE_REVISION_STD_REGISTER:
                send_reg_value1(NODE_HARDWARE_REVISION_STD_REGISTER, cm->source_id, cm->unicast.seqnum, node_info.hardware_revision);
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
            case NODE_RESET_REASON_STD_REGISTER:
                send_reg_value1(NODE_RESET_REASON_STD_REGISTER, cm->source_id, cm->unicast.seqnum, node_info.reset_reason);
                return;
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
                    CAN_send_node_info_broadcast(0);
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
            asm ("goto 0xf600");
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
