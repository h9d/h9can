/*
 * h9pic-bootloader
 *
 * Created by SQ8KFH on 2020-07-29.
 *
 * Copyright (C) 2020 Kamil Palkowski. All rights reserved.
 */

#include "can.h"
#include "config.h"

#include <h9pic/common.h>
#include <h9pic/node_id.h>

uint8_t can_node_id = 255;

#if defined(_18FXXQ84_FAMILY_)

/*
 * PIC18F-Q84 CAN FD module in Normal CAN 2.0 mode, 125 kbps, extended frames only (same setup as h9can/pic/18FXXQ84/canfd.c).
 * Message memory (TEF and TXQ disabled), 16 B per object (8 B header + 8 B payload):
 *   FIFO 1 - RX, bootloader frames to this node (filter 0)
 *   FIFO 2 - not used by the bootloader
 *   FIFO 3 - TX
 */

#define CANFD_MODE_NORMAL_20    6   // REQOP / OPMOD: Normal CAN 2.0, error frames on CAN FD frames
#define CANFD_MODE_CONFIG       4

#define CANFD_FIFO_DEPTH        8
#define CANFD_RAM_ADDR          0x3800      // CAN message RAM 0x3800-0x3FFF (CxFIFOBA must be >= 0x3800), not used by the compiler

static void canfd_set_mode(uint8_t mode) {
    C1CONTbits.REQOP = mode;
    while (C1CONUbits.OPMOD != mode);
}

/*
 * ECAN id registers (calc_can_*_id: SIDH, SIDL, EIDH, EIDL) -> filter object / mask / message object bytes 0..3
 * (SID[10:0] | EID[17:0] << 11), byte operations only.
 */
static void canfd_id(const uint8_t *ecan_id, volatile uint8_t *obj) {
    obj[0] = (uint8_t)(ecan_id[0] << 3) | (ecan_id[1] >> 5);                 // SID[7:0]
    obj[1] = (ecan_id[0] >> 5) | (uint8_t)(ecan_id[3] << 3);                 // SID[10:8], EID[4:0]
    obj[2] = (ecan_id[3] >> 5) | (uint8_t)(ecan_id[2] << 3);                 // EID[12:5]
    obj[3] = (ecan_id[2] >> 5) | (uint8_t)((ecan_id[1] & 0x03) << 3)         // EID[17:13]
             | ((ecan_id[1] & 0x08) ? _C1FLTOBJ0T_EXIDE_MASK : 0);          // EXIDE (MIDE in the mask)
}

// FIFO user address (CxFIFOUAn) -> message object
static volatile uint8_t *canfd_obj(uint16_t ua) {
    if (ua >= CANFD_RAM_ADDR)       // absolute RAM address (FIFOBA + offset)
        ua -= CANFD_RAM_ADDR;
    return (volatile uint8_t *)(CANFD_RAM_ADDR + ua);
}

void CAN_init(void) {
    can_node_id = read_node_id();

    LATBbits.LATB2 = 1;             // recessive until the module drives the pin
    RB2PPS = 0x46;                  // CANTX
    CANRXPPS = 0x0B;                // RB3
    ANSELBbits.ANSELB3 = 0;
    TRISBbits.TRISB3 = 1;
    TRISBbits.TRISB2 = 0;

    C1CONHbits.ON = 1;
    canfd_set_mode(CANFD_MODE_CONFIG);

    C1CONL = 0x00;                  // CLKSEL0 = 0: FOSC, DNCNT 0
    C1CONH = _C1CONH_ON_MASK | _C1CONH_BRSDIS_MASK;
    C1CONU = 0x00;                  // TXQEN = 0, STEF = 0, RTXAT = 0 (unlimited retransmissions)

    /**
        Baud rate: 125kbps
        CAN clock: FOSC = 16000000
        Time quanta: 32 x 250ns (BRP = 3)
        Bit time: 1 + 23 + 8
        Sample point: 75%
    */
    C1NBTCFGT = 3;                  // BRP
    C1NBTCFGU = 22;                 // TSEG1
    C1NBTCFGH = 7;                  // TSEG2
    C1NBTCFGL = 7;                  // SJW

    C1FIFOBA = CANFD_RAM_ADDR;

    // PLSIZE = 0 (8 B), FSIZE = depth - 1
    C1FIFOCON1L = 0x00;                             // RX
    C1FIFOCON1T = CANFD_FIFO_DEPTH - 1;
    C1FIFOCON3L = _C1FIFOCON3L_TXEN_MASK;           // TX
    C1FIFOCON3T = CANFD_FIFO_DEPTH - 1;

    // filters are undefined after reset, disable all
    C1FLTCON0L = 0; C1FLTCON0H = 0; C1FLTCON0U = 0; C1FLTCON0T = 0;
    C1FLTCON1L = 0; C1FLTCON1H = 0; C1FLTCON1U = 0; C1FLTCON1T = 0;
    C1FLTCON2L = 0; C1FLTCON2H = 0; C1FLTCON2U = 0; C1FLTCON2T = 0;

    // filter 0 -> FIFO 1: bootloader frames to this node
    uint8_t mask[4], id[4];
    calc_can_unicast_id(&mask[0], &mask[1], &mask[2], &mask[3], H9FRAME_BOOTLOADER_MSG_TYPE_GROUP_MASK, 0, 0, H9FRAME_ID_MASK, 0);
    calc_can_unicast_id(&id[0], &id[1], &id[2], &id[3], H9FRAME_BOOTLOADER_MSG_TYPE_GROUP, 0, 0, can_node_id, 0);
    canfd_id(id, &C1FLTOBJ0L);
    canfd_id(mask, &C1MASK0L);
    C1FLTCON0Lbits.F0BP = 1;        // FIFO 1
    C1FLTCON0Lbits.FLTEN0 = 1;

    canfd_set_mode(CANFD_MODE_NORMAL_20);
}

void CAN_put_msg_blocking(h9frame_t *cm) {
    uint8_t id[4];

    while (!C1FIFOSTA3Lbits.TFNRFNIF);  // TX FIFO full

    cm->source_id = can_node_id;

    if (cm->type & H9FRAME_UNICAST_BROADCAST_BIT) {
        calc_can_broadcast_id(&id[0], &id[1], &id[2], &id[3], cm->type, cm->source_id, cm->broadcast.group);
    }
    else
        calc_can_unicast_id(&id[0], &id[1], &id[2], &id[3], cm->type, cm->source_id, cm->unicast.flags, cm->unicast.destination_id, cm->unicast.seqnum);

    volatile uint8_t *obj = canfd_obj((uint16_t)C1FIFOUA3);

    canfd_id(id, obj);
    obj[3] &= 0x1f;                 // EID[17:13] only (SID11 = 0)
    obj[4] = 0x10 | (cm->dlc & 0x0f);   // IDE, CAN 2.0 frame
    obj[5] = 0;
    obj[6] = 0;
    obj[7] = 0;
    for (uint8_t i = 0; i < 8; ++i)
        obj[8 + i] = cm->data[i];

    C1FIFOCON3H |= _C1FIFOCON3H_UINC_MASK | _C1FIFOCON3H_TXREQ_MASK;
}

/**
 * @warning Support only unicast frame!
 */
uint8_t CAN_get_msg_blocking(h9frame_t *cm) {
    uint24_t timeout_counter = 0x1fffff;

    while (timeout_counter) {
        if (C1FIFOSTA1Lbits.TFNRFNIF) {     // RX FIFO not empty
            volatile uint8_t *obj = canfd_obj((uint16_t)C1FIFOUA1);

            cm->dlc = obj[4] & 0x0f;        // R1[3:0] (R1[15:11] = FILHIT)

            // SID[10:0] = obj[1][2:0] obj[0], EID[17:0] = obj[3][4:0] obj[2] obj[1][7:3]
            uint8_t sidh = (uint8_t)((obj[1] & 0x07) << 5) | (obj[0] >> 3);             // SID[10:3]
            uint8_t sidl = (uint8_t)(obj[0] << 5) | ((obj[3] >> 3) & 0x03);             // SID[2:0], EID[17:16]
            uint8_t eidh = (uint8_t)((obj[3] & 0x07) << 5) | (obj[2] >> 3);             // EID[15:8]
            uint8_t eidl = (uint8_t)((obj[2] & 0x07) << 5) | (obj[1] >> 3);             // EID[7:0]
            for (uint8_t i = 0; i < 8; ++i)
                cm->data[i] = obj[8 + i];

            C1FIFOCON1Hbits.UINC = 1;

            cm->type = sidh >> 3;
            cm->source_id = (uint8_t)(sidh << 5) | (uint8_t)(sidl >> 3 & 0x1c) | (uint8_t)(sidl & 0x03);

            cm->unicast.flags = eidh >> 5;
            cm->unicast.destination_id = (uint8_t)(eidh << 3) | (uint8_t)(eidl >> 5);
            cm->unicast.seqnum = eidl & 0x1f;
            return 1;
        }
        --timeout_counter;
    }
    return 0;
}
#else


void CAN_init(void) {
    can_node_id = read_node_id();
    
    CANCON = 0b10000000;
    while (0x80 != (CANSTAT & 0xE0));
    
    ECANCON = 0x00;
    
    CIOCON = 0x21; // ?? 1 clock source
    
    //mask for RXF0
    calc_can_unicast_id(&RXM0SIDH, &RXM0SIDL, &RXM0EIDH, &RXM0EIDL, H9FRAME_BOOTLOADER_MSG_TYPE_GROUP_MASK, 0, 0, H9FRAME_ID_MASK, 0);
    calc_can_unicast_id(&RXF0SIDH, &RXF0SIDL, &RXF0EIDH, &RXF0EIDL, H9FRAME_BOOTLOADER_MSG_TYPE_GROUP, 0, 0, can_node_id, 0);
    
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
    
    CANCON = 0;
    
    RXB0CON = 0b01000000;
    //RXB1CON = 0b01000000;
    
    while (CANSTATbits.OPMODE0);
}

void CAN_put_msg_blocking(h9frame_t *cm) {
    while (TXB0CONbits.TXREQ);
    
    cm->source_id = can_node_id;
    
    if (cm->type & H9FRAME_UNICAST_BROADCAST_BIT) {
        calc_can_broadcast_id(&TXB0SIDH, &TXB0SIDL, &TXB0EIDH, &TXB0EIDL, cm->type, cm->source_id, cm->broadcast.group);
    }
    else
        calc_can_unicast_id(&TXB0SIDH, &TXB0SIDL, &TXB0EIDH, &TXB0EIDL, cm->type, cm->source_id, cm->unicast.flags, cm->unicast.destination_id, cm->unicast.seqnum);
    
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

/**
 * @warning Support only unicast frame!
 */
uint8_t CAN_get_msg_blocking(h9frame_t *cm) {
    uint24_t timeout_counter = 0x1fffff;
    
    while (timeout_counter) {
        if (RXB0CONbits.RXFUL) {
            cm->type = RXB0SIDH >> 3;
            cm->source_id = (uint8_t)(RXB0SIDH << 5) | (uint8_t)(RXB0SIDL >> 3 & 0x1c) | (uint8_t)(RXB0SIDL & 0x03);
        
            cm->unicast.flags = RXB0EIDH >> 5;
            cm->unicast.destination_id = (uint8_t)(RXB0EIDH << 3) | (uint8_t)(RXB0EIDL >> 5);
            cm->unicast.seqnum = RXB0EIDL & 0x1f;     

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
        --timeout_counter;
    }
    return 0;
}
#endif
