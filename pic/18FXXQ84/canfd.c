/*
 * h9pic-can, PIC18F-Q84 CAN FD module driver.
 *
 * The module runs in Normal CAN 2.0 mode, 125 kbps, extended frames only.
 * Frame ids are built by calc_can_*_id (h9pic/common.h) in the ECAN SIDH/SIDL/EIDH/EIDL layout and
 * converted here.
 *
 * Message memory (TEF disabled), 16 B per object (8 B header + 8 B payload), 832 B:
 *   TXQ    - TX, 4 objects, NODE_FAULT frames (TXPRI 1, sent before FIFO 3)
 *   FIFO 1 - RX, 16 objects, unicast to this node (filter 0)
 *   FIFO 2 - RX, 16 objects, broadcasts (filters 1-4)
 *   FIFO 3 - TX, 16 objects, all other frames (TXPRI 0, sent in order)
 * Received frames stay in the RX FIFOs until CAN_get_msg() reads them (no RX interrupt, no software buffer).
 */

#include <xc.h>

#if defined(_18FXXQ84_FAMILY_)

#include "../hw.h"
#include "h9pic/can.h"
#include "h9pic/common.h"
#include "h9frame.h"

#define CANFD_MODE_NORMAL_20    6   // REQOP / OPMOD: Normal CAN 2.0, error frames on CAN FD frames
#define CANFD_MODE_CONFIG       4

#define CANFD_FIFO_RX_UNICAST   1
#define CANFD_FIFO_RX_BROADCAST 2

#define CANFD_FIFO_DEPTH        16          // FIFO 1-3, 1-32 objects (FSIZE + 1)
#define CANFD_TXQ_DEPTH         4
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

/*
 * fltcon: C1FLTCONm L/H/U/T byte of the filter, obj / mask: C1FLTOBJnL / C1MASKnL. Routed to fifo, enabled.
 * The filter object can be changed only when the filter is disabled, also in Normal mode.
 */
static void canfd_set_filter(volatile uint8_t *fltcon, volatile uint8_t *obj, volatile uint8_t *mask,
                             uint8_t fifo, const uint8_t *ecan_id, const uint8_t *ecan_mask) {
    *fltcon = 0;
    canfd_id(ecan_id, obj);
    canfd_id(ecan_mask, mask);
    *fltcon = _C1FLTCON0L_FLTEN0_MASK | fifo;  // FLTENn | FnBP (same layout in every byte)
}

// FIFO user address (CxFIFOUAn) -> message object
static volatile uint8_t *canfd_obj(uint16_t ua) {
    if (ua >= CANFD_RAM_ADDR)       // absolute RAM address (FIFOBA + offset)
        ua -= CANFD_RAM_ADDR;
    return (volatile uint8_t *)(CANFD_RAM_ADDR + ua);
}

/*
 * RX message object -> ECAN layout: ecan_id[0..3] = SIDH, SIDL, EIDH, EIDL, returns DLC.
 * The caller releases the object (CxFIFOCONnH.UINC).
 */
static uint8_t canfd_rx_read(volatile uint8_t *obj, uint8_t *ecan_id, uint8_t *data) {
    uint8_t dlc = obj[4] & 0x0f;   // R1[3:0] (R1[15:11] = FILHIT)

    ecan_id[0] = (uint8_t)((obj[1] & 0x07) << 5) | (obj[0] >> 3);           // SID[10:3]
    ecan_id[1] = (uint8_t)(obj[0] << 5) | 0x08 | ((obj[3] >> 3) & 0x03);    // SID[2:0], EXIDE, EID[17:16]
    ecan_id[2] = (uint8_t)((obj[3] & 0x07) << 5) | (obj[2] >> 3);           // EID[15:8]
    ecan_id[3] = (uint8_t)((obj[2] & 0x07) << 5) | (obj[1] >> 3);           // EID[7:0]
    for (uint8_t i = 0; i < 8; ++i)
        data[i] = obj[8 + i];

    return dlc;
}

uint8_t can_hw_rx(can_buf_t *frame) {
    if (C1FIFOSTA1Lbits.TFNRFNIF) {         // unicast FIFO not empty
        frame->dlc = canfd_rx_read(canfd_obj((uint16_t)C1FIFOUA1), frame->id, frame->data);
        C1FIFOCON1Hbits.UINC = 1;           // release the object
        return 1;
    }
    if (C1FIFOSTA2Lbits.TFNRFNIF) {         // broadcast FIFO not empty
        frame->dlc = canfd_rx_read(canfd_obj((uint16_t)C1FIFOUA2), frame->id, frame->data);
        C1FIFOCON2Hbits.UINC = 1;
        return 1;
    }
    return 0;
}

// TX message object (TXQ / FIFO 3 user address) <- extended frame, the caller sets UINC | TXREQ
static void canfd_tx_write(volatile uint8_t *obj, const uint8_t *ecan_id, uint8_t dlc, const uint8_t *data) {
    canfd_id(ecan_id, obj);
    obj[3] &= 0x1f;                 // EID[17:13] only (SID11 = 0)
    obj[4] = 0x10 | (dlc & 0x0f);   // IDE, CAN 2.0 frame
    obj[5] = 0;
    obj[6] = 0;
    obj[7] = 0;
    for (uint8_t i = 0; i < 8; ++i)
        obj[8 + i] = data[i];
}

static void canfd_txq_write(const uint8_t *ecan_id, uint8_t dlc, const uint8_t *data) {
    canfd_tx_write(canfd_obj((uint16_t)C1TXQUA), ecan_id, dlc, data);
    C1TXQCONH |= _C1TXQCONH_UINC_MASK | _C1TXQCONH_TXREQ_MASK;
}

void can_hw_init(uint8_t node_id, uint16_t node_type) {
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
    C1CONU = _C1CONU_TXQEN_MASK;    // TXQ, STEF = 0, RTXAT = 0 (unlimited retransmissions)

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
    C1TXQCONL = 0x00;                               // TX (TXEN is fixed to 1), no interrupts
    C1TXQCONT = CANFD_TXQ_DEPTH - 1;
    C1TXQCONUbits.TXPRI = 1;                        // before FIFO 3
    C1FIFOCON1L = 0x00;                             // RX, no interrupts
    C1FIFOCON1T = CANFD_FIFO_DEPTH - 1;
    C1FIFOCON2L = 0x00;                             // RX, no interrupts
    C1FIFOCON2T = CANFD_FIFO_DEPTH - 1;
    C1FIFOCON3L = _C1FIFOCON3L_TXEN_MASK;           // TX
    C1FIFOCON3T = CANFD_FIFO_DEPTH - 1;
    C1FIFOCON3Ubits.TXPRI = 0;

    // filters are undefined after reset, disable all
    C1FLTCON0L = 0; C1FLTCON0H = 0; C1FLTCON0U = 0; C1FLTCON0T = 0;
    C1FLTCON1L = 0; C1FLTCON1H = 0; C1FLTCON1U = 0; C1FLTCON1T = 0;
    C1FLTCON2L = 0; C1FLTCON2H = 0; C1FLTCON2U = 0; C1FLTCON2T = 0;

    // filter 0 -> FIFO 1: unicast to this node
    uint8_t mask[4], id[4];
    calc_can_unicast_id(&mask[0], &mask[1], &mask[2], &mask[3], H9FRAME_UNICAST_MSG_TYPE_GROUP_MASK, 0, 0, H9FRAME_ID_MASK, 0);
    calc_can_unicast_id(&id[0], &id[1], &id[2], &id[3], H9FRAME_UNICAST_MSG_TYPE_GROUP, 0, 0, node_id, 0);
    canfd_set_filter(&C1FLTCON0L, &C1FLTOBJ0L, &C1MASK0L, CANFD_FIFO_RX_UNICAST, id, mask);

    // filters 1, 2 -> FIFO 2: special broadcasts (DISCOVER, GROUP_RESET) to all nodes and to this node type
    calc_can_broadcast_id(&mask[0], &mask[1], &mask[2], &mask[3], H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP_MASK, 0, H9FRAME_NODE_TYPE_MASK);
    calc_can_broadcast_id(&id[0], &id[1], &id[2], &id[3], H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP, 0, H9FRAME_BROADCAST_ALL_GROUP);
    canfd_set_filter(&C1FLTCON0H, &C1FLTOBJ1L, &C1MASK1L, CANFD_FIFO_RX_BROADCAST, id, mask);
    calc_can_broadcast_id(&id[0], &id[1], &id[2], &id[3], H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP, 0, node_type);
    canfd_set_filter(&C1FLTCON0U, &C1FLTOBJ2L, &C1MASK2L, CANFD_FIFO_RX_BROADCAST, id, mask);

    // filters 3, 4 -> FIFO 2: all broadcast types for a given group, disabled until CAN_set_msg_filter_1/2

    canfd_set_mode(CANFD_MODE_NORMAL_20);
}

void can_hw_set_broadcast_filter(uint8_t n, uint16_t broadcast_group) {
    uint8_t mask[4], id[4];
    calc_can_broadcast_id(&mask[0], &mask[1], &mask[2], &mask[3], H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP_MASK, 0, H9FRAME_NODE_TYPE_MASK);
    calc_can_broadcast_id(&id[0], &id[1], &id[2], &id[3], H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP, 0, broadcast_group);
    if (n == 1)
        canfd_set_filter(&C1FLTCON0T, &C1FLTOBJ3L, &C1MASK3L, CANFD_FIFO_RX_BROADCAST, id, mask);
    else
        canfd_set_filter(&C1FLTCON1L, &C1FLTOBJ4L, &C1MASK4L, CANFD_FIFO_RX_BROADCAST, id, mask);
}

uint8_t can_hw_error_warning(void) {
    return C1TRECUbits.TXWARN | C1TRECUbits.RXWARN;
}

uint8_t can_hw_bus_error(void) {
    return C1TRECUbits.RXBP || C1TRECUbits.TXBP || C1TRECUbits.TXBO;
}

uint8_t can_hw_rx_overflow(void) {
    uint8_t overflow = C1FIFOSTA1Lbits.RXOVIF || C1FIFOSTA2Lbits.RXOVIF;
    if (overflow) {
        C1FIFOSTA1Lbits.RXOVIF = 0;
        C1FIFOSTA2Lbits.RXOVIF = 0;
    }
    return overflow;
}

uint8_t can_hw_tx(const uint8_t *id, uint8_t dlc, const uint8_t *data, uint8_t urgent) {
    if (urgent) {
        if (!C1TXQSTALbits.TXQNIF)  // TXQ full
            return 0;
        canfd_txq_write(id, dlc, data);
        return 1;
    }

    if (!C1FIFOSTA3Lbits.TFNRFNIF)  // TX FIFO full
        return 0;
    canfd_tx_write(canfd_obj((uint16_t)C1FIFOUA3), id, dlc, data);
    C1FIFOCON3H |= _C1FIFOCON3H_UINC_MASK | _C1FIFOCON3H_TXREQ_MASK;
    return 1;
}

void can_hw_tx_on_bus_error(const uint8_t *id, uint8_t dlc, const uint8_t *data) {
    // frames in FIFO 3 are kept and sent after bus-off recovery, behind the fault frame
    if (C1TXQSTALbits.TXQEIF)       // no fault frame waiting yet
        canfd_txq_write(id, dlc, data);
}

#endif /* _18FXXQ84_FAMILY_ */
