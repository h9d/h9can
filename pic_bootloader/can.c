/*
 * h9pic-bootloader
 *
 * Created by SQ8KFH on 2020-07-29.
 *
 * Copyright (C) 2020 Kamil Palkowski. All rights reserved.
 */

#include "can.h"
#include "config.h"


volatile uint16_t can_node_id = 341;


static void set_CAN_id(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id, uint8_t *passedInEIDH, uint8_t *passedInEIDL, uint8_t *passedInSIDH, uint8_t *passedInSIDL);
static uint8_t process_msg(h9msg_t *cm);
static void read_node_id(void) ;


void CAN_init(void) {
    read_node_id();
    
    CANCON = 0b10000000;
    while (0x80 != (CANSTAT & 0xE0));
    
    ECANCON = 0x00;
    
    CIOCON = 0x21; // ?? 1 clock source

    uint8_t tempEIDH = 0;
    uint8_t tempEIDL = 0;
    uint8_t tempSIDH = 0;
    uint8_t tempSIDL = 0;
    
    set_CAN_id(0, H9MSG_TYPE_SUBGROUP_MASK, 0, (1<<H9MSG_DESTINATION_ID_BIT_LENGTH)-1, 0, &tempEIDH, &tempEIDL, &tempSIDH, &tempSIDL);
    RXM0EIDH = tempEIDH;
    RXM0EIDL = tempEIDL;
    RXM0SIDH = tempSIDH;
    RXM0SIDL = tempSIDL;
    
    set_CAN_id(0, H9MSG_TYPE_GROUP_0, 0, can_node_id, 0, &tempEIDH, &tempEIDL, &tempSIDH, &tempSIDL);
    RXF0EIDH = tempEIDH;
    RXF0EIDL = tempEIDL;
    RXF0SIDH = tempSIDH;
    RXF0SIDL = tempSIDL;
    RXF1EIDH = tempEIDH;
    RXF1EIDL = tempEIDL;
    RXF1SIDH = tempSIDH;
    RXF1SIDL = tempSIDL;
    
//    RXM1EIDH = 0x00;
//    RXM1EIDL = 0x00;
//    RXM1SIDH = 0x00;
//    RXM1SIDL = 0x00;
//    
//    RXF2EIDH = 0x00;
//    RXF2EIDL = 0x00;
//    RXF2SIDH = 0x00;
//    RXF2SIDL = 0x08;
//    RXF3EIDH = 0x00;
//    RXF3EIDL = 0x00;
//    RXF3SIDH = 0x00;
//    RXF3SIDL = 0x08;
//    RXF4EIDH = 0x00;
//    RXF4EIDL = 0x00;
//    RXF4SIDH = 0x00;
//    RXF4SIDL = 0x08;
//    RXF5EIDH = 0x00;
//    RXF5EIDL = 0x00;
//    RXF5SIDH = 0x00;
//    RXF5SIDL = 0x08;
    
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
    
//    PIR5bits.RXB0IF = 0;
//    PIE5bits.RXB0IE = 1;
//    IPR5bits.RXB0IP = 0;
//    
//    PIR5bits.RXB1IF = 0;
//    PIE5bits.RXB1IE = 1;
//    IPR5bits.RXB1IP = 0;
    
    CANCON = 0;
    
    RXB0CON = 0b01000000;
    //RXB1CON = 0b01000000;
    
    while (CANSTATbits.OPMODE0);
}

void CAN_put_msg_blocking(h9msg_t *cm) {
    uint8_t tempEIDH = 0;
    uint8_t tempEIDL = 0;
    uint8_t tempSIDH = 0;
    uint8_t tempSIDL = 0;
    
    set_CAN_id(cm->priority, cm->type, cm->seqnum, cm->destination_id, cm->source_id, &tempEIDH, &tempEIDL, &tempSIDH, &tempSIDL);
    
    while (TXB0CONbits.TXREQ);
    
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

//          | SIDH                    | SIDL                    | EIDH                    | EIDL
// 31 30 29 | 28 27 26 25 24 23 22 21 | 20 19 18 ** ** ** 17 16 | 15 14 13 12 11 10 09 08 | 07 06 05 04 03 02 01 00
// -- -- -- | pp ty ty ty ty ty se se | se se se **  1 ** ds ds | ds ds ds ds ds ds ds so | so so so so so so so so
uint8_t CAN_get_msg_blocking(h9msg_t*cm) {
    uint24_t timeout_counter = 0x1fffff;
    
    while (timeout_counter) {
        if (RXB0CONbits.RXFUL) {
            cm->priority = (RXB0SIDH >> 7) & 0x01;
            cm->type = (RXB0SIDH >> 2) & 0x1f;
            cm->seqnum = ((RXB0SIDH << 3) & 0x18) | ((RXB0SIDL >> 5) & 0x07);
            cm->destination_id = ((RXB0SIDL  & 0x03) << 7) | ((RXB0EIDH >> 1) & 0x7f);
            cm->source_id = ((RXB0EIDH & 0x1) << 8) | RXB0EIDL;

            cm->dlc = RXB0DLC & 0x0f;

            cm->data[0] = RXB0D0;
            cm->data[1] = RXB0D1;
            cm->data[2] = RXB0D2;
            cm->data[3] = RXB0D3;
            cm->data[4] = RXB0D4;
            cm->data[5] = RXB0D5;
            cm->data[6] = RXB0D6;
            cm->data[7] = RXB0D7;

            RXB0CONbits.RXFUL = 0;
            return 1;
        }
//        else if (RXB1CONbits.RXFUL) {
//            cm->priority = (RXB1SIDH >> 7) & 0x01;
//            cm->type = (RXB1SIDH >> 2) & 0x1f;
//            cm->seqnum = ((RXB1SIDH << 3) & 0x18) | ((RXB1SIDL >> 5) & 0x07);
//            cm->destination_id = ((RXB1SIDL  & 0x03) << 7) | ((RXB1EIDH >> 1) & 0x7f);
//            cm->source_id = ((RXB1EIDH & 0x1) << 8) | RXB1EIDL;
//
//            cm->dlc = RXB1DLC & 0x0f;
//
//            cm->data[0] = RXB1D0;
//            cm->data[1] = RXB1D1;
//            cm->data[2] = RXB1D2;
//            cm->data[3] = RXB1D3;
//            cm->data[4] = RXB1D4;
//            cm->data[5] = RXB1D5;
//            cm->data[6] = RXB1D6;
//            cm->data[7] = RXB1D7;
//
//            RXB1CONbits.RXFUL = 0;
//            break;
//        }
        --timeout_counter;
    }
    return 0;
}

uint8_t DATAEE_ReadByte(uint16_t bAdd) {
    EEADRH = ((bAdd >> 8) & 0x03);
    EEADR = (bAdd & 0xFF);
    EECON1bits.CFGS = 0;
    EECON1bits.EEPGD = 0;
    EECON1bits.RD = 1;
    NOP();  // NOPs may be required for latency at high frequencies
    NOP();

    return (EEDATA);
}

static void read_node_id(void) {
    uint16_t node_id = DATAEE_ReadByte(0x0000);
    node_id <<= 8;
    node_id |= DATAEE_ReadByte(0x01);
    if (node_id > 0 && node_id < H9MSG_BROADCAST_ID) {
        can_node_id = node_id & ((1<<H9MSG_SOURCE_ID_BIT_LENGTH)-1);
    }
    else {
        can_node_id = 0;
    }
}

//          | SIDH                    | SIDL                    | EIDH                    | EIDL
// 31 30 29 | 28 27 26 25 24 23 22 21 | 20 19 18 ** ** ** 17 16 | 15 14 13 12 11 10 09 08 | 07 06 05 04 03 02 01 00
// -- -- -- | pp ty ty ty ty ty se se | se se se **  1 ** ds ds | ds ds ds ds ds ds ds so | so so so so so so so so
static void set_CAN_id(uint8_t priority, uint8_t type, uint8_t seqnum, uint16_t destination_id, uint16_t source_id, uint8_t *passedInEIDH, uint8_t *passedInEIDL, uint8_t *passedInSIDH, uint8_t *passedInSIDL) {
    *passedInSIDH = ((priority << 7) & 0x80) | ((type << 2) & 0x7c) | ((seqnum >> 3) & 0x03);
    *passedInSIDL = ((seqnum << 5) & 0xe0) | 0x08 | ((destination_id >> 7) & 0x03);
    *passedInEIDH = ((destination_id << 1) & 0xfe) | ((source_id >> 8) & 0x01);
    *passedInEIDL = (source_id & 0xff);
}
