/*
 * h9pic-can, PIC18F46K80 ECAN driver (Mode 0, legacy).
 */

#include <xc.h>

#if defined(_18F66K80_FAMILY_)

#include "../hw.h"
#include "h9pic/can.h"
#include "h9pic/common.h"
#include "h9frame.h"

#define CAN_MODE_NORMAL 0x00
#define CAN_MODE_CONFIG 0x80

static void can_set_mode(uint8_t mode) {
    CANCON = mode;
    while ((CANSTAT & 0xE0) != mode);
}

void can_interrupt(void) {
    if (PIE5bits.RXB0IE && PIR5bits.RXB0IF) {
        PIR5bits.RXB0IF = 0;
        can_buf_t *b = can_rx_slot();
        if (b != NULL) {
            b->id[0] = RXB0SIDH;
            b->id[1] = RXB0SIDL;
            b->id[2] = RXB0EIDH;
            b->id[3] = RXB0EIDL;
            b->dlc = RXB0DLC;
            b->data[0] = RXB0D0;
            b->data[1] = RXB0D1;
            b->data[2] = RXB0D2;
            b->data[3] = RXB0D3;
            b->data[4] = RXB0D4;
            b->data[5] = RXB0D5;
            b->data[6] = RXB0D6;
            b->data[7] = RXB0D7;

            can_rx_commit();
        }
        RXB0CONbits.RXFUL = 0;
    }
    if (PIE5bits.RXB1IE && PIR5bits.RXB1IF) {
        PIR5bits.RXB1IF = 0;
        can_buf_t *b = can_rx_slot();
        if (b != NULL) {
            b->id[0] = RXB1SIDH;
            b->id[1] = RXB1SIDL;
            b->id[2] = RXB1EIDH;
            b->id[3] = RXB1EIDL;
            b->dlc = RXB1DLC;
            b->data[0] = RXB1D0;
            b->data[1] = RXB1D1;
            b->data[2] = RXB1D2;
            b->data[3] = RXB1D3;
            b->data[4] = RXB1D4;
            b->data[5] = RXB1D5;
            b->data[6] = RXB1D6;
            b->data[7] = RXB1D7;

            can_rx_commit();
        }
        RXB1CONbits.RXFUL = 0;
    }
}

void can_hw_init(uint8_t node_id, uint16_t node_type) {
    TRISBbits.TRISB2 = 1; //CANTX ax output
    TRISBbits.TRISB3 = 1; //CANRX ax input

    can_set_mode(CAN_MODE_CONFIG);

    ECANCON = 0x00;

    CIOCON = 0x21; // ?? 1 clock source

    // Mode 0: RXM0 is shared by RXF0 - RXF1 (RXB0), RXM1 by RXF2 - RXF5 (RXB1).
    // Filter registers are undefined after reset and all six are always enabled, so every one must be set.

    //RXB0: unicast to this node (RXF1 duplicates RXF0)
    calc_can_unicast_id(&RXM0SIDH, &RXM0SIDL, &RXM0EIDH, &RXM0EIDL, H9FRAME_UNICAST_MSG_TYPE_GROUP_MASK, 0, 0, H9FRAME_ID_MASK, 0);
    calc_can_unicast_id(&RXF0SIDH, &RXF0SIDL, &RXF0EIDH, &RXF0EIDL, H9FRAME_UNICAST_MSG_TYPE_GROUP, 0, 0, node_id, 0);
    calc_can_unicast_id(&RXF1SIDH, &RXF1SIDL, &RXF1EIDH, &RXF1EIDL, H9FRAME_UNICAST_MSG_TYPE_GROUP, 0, 0, node_id, 0);

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
}

// Filter registers are writable in Configuration mode only
void can_hw_set_broadcast_filter(uint8_t n, uint16_t broadcast_group) {
    can_set_mode(CAN_MODE_CONFIG);
    if (n == 1)
        calc_can_broadcast_id(&RXF4SIDH, &RXF4SIDL, &RXF4EIDH, &RXF4EIDL, H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP, 0, broadcast_group);
    else
        calc_can_broadcast_id(&RXF5SIDH, &RXF5SIDL, &RXF5EIDH, &RXF5EIDL, H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP, 0, broadcast_group);
    can_set_mode(CAN_MODE_NORMAL);
}

uint8_t can_hw_error_warning(void) {
    return COMSTATbits.TXWARN | COMSTATbits.RXWARN;
}

uint8_t can_hw_bus_error(void) {
    return COMSTATbits.RXBP == 1 || COMSTATbits.TXBP == 1 || COMSTATbits.TXBO == 1;
}

uint8_t can_hw_rx_overflow(void) {
    uint8_t overflow = COMSTATbits.RXB0OVFL || COMSTATbits.RXB1OVFL;
    if (overflow) {
        COMSTATbits.RXB0OVFL = 0;
        COMSTATbits.RXB1OVFL = 0;
    }
    return overflow;
}

static void tx_write_txb2(const uint8_t *id, uint8_t dlc, const uint8_t *data) {
    TXB2SIDH = id[0];
    TXB2SIDL = id[1];
    TXB2EIDH = id[2];
    TXB2EIDL = id[3];
    TXB2DLC  = dlc;
    TXB2D0   = data[0];
    TXB2D1   = data[1];
    TXB2D2   = data[2];
    TXB2D3   = data[3];
    TXB2D4   = data[4];
    TXB2D5   = data[5];
    TXB2D6   = data[6];
    TXB2D7   = data[7];
    TXB2CONbits.TXREQ = 1;
}

uint8_t can_hw_tx(const uint8_t *id, uint8_t dlc, const uint8_t *data) {
    if (TXB0CONbits.TXREQ != 1) {
        TXB0SIDH = id[0];
        TXB0SIDL = id[1];
        TXB0EIDH = id[2];
        TXB0EIDL = id[3];
        TXB0DLC  = dlc;
        TXB0D0   = data[0];
        TXB0D1   = data[1];
        TXB0D2   = data[2];
        TXB0D3   = data[3];
        TXB0D4   = data[4];
        TXB0D5   = data[5];
        TXB0D6   = data[6];
        TXB0D7   = data[7];
        TXB0CONbits.TXREQ = 1;
        return 1;
    }
    else if (TXB1CONbits.TXREQ != 1) {
        TXB1SIDH = id[0];
        TXB1SIDL = id[1];
        TXB1EIDH = id[2];
        TXB1EIDL = id[3];
        TXB1DLC  = dlc;
        TXB1D0   = data[0];
        TXB1D1   = data[1];
        TXB1D2   = data[2];
        TXB1D3   = data[3];
        TXB1D4   = data[4];
        TXB1D5   = data[5];
        TXB1D6   = data[6];
        TXB1D7   = data[7];
        TXB1CONbits.TXREQ = 1;
        return 1;
    }
    else if (TXB2CONbits.TXREQ != 1) {
        tx_write_txb2(id, dlc, data);
        return 1;
    }
    return 0;
}

void can_hw_tx_on_bus_error(const uint8_t *id, uint8_t dlc, const uint8_t *data) {
    TXB2CONbits.TXREQ = 0;
    while (TXB2CONbits.TXREQ);
    tx_write_txb2(id, dlc, data);   // ramka czeka w TXB2 i pojedzie automatycznie po wyjściu z bus-off
}

#endif /* _18F66K80_FAMILY_ */
