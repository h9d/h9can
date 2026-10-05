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
#include <h9pic/ee_mem.h>

uint8_t can_node_id = 255;

static uint8_t process_msg(h9frame_t *cm);


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
        cm->broadcast.group = H9FRAME_BROADCAST_BOOTLOADER_GROUP;
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
