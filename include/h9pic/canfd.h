/*
 * PIC18F-Q84 CAN FD module helpers, shared by the PIC library and the PIC bootloader.
 *
 * The module runs in Normal CAN 2.0 mode, 125 kbps, extended frames only.
 * Frame ids are built by calc_can_*_id (h9pic/common.h) in the ECAN SIDH/SIDL/EIDH/EIDL layout and
 * converted here, so the h9 id layout is described in one place for both MCUs.
 *
 * Message memory (TEF and TXQ disabled), 16 B per object (8 B header + 8 B payload):
 *   FIFO 1 - RX, unicast to this node (filter 0)
 *   FIFO 2 - RX, broadcasts (filters 1-4)
 *   FIFO 3 - TX
 */

#ifndef H9PIC_CANFD_H
#define H9PIC_CANFD_H

#include <xc.h>
#include <stdint.h>

#define CANFD_MODE_NORMAL_20    6   // REQOP / OPMOD: Normal CAN 2.0, error frames on CAN FD frames
#define CANFD_MODE_CONFIG       4

#define CANFD_FIFO_RX_UNICAST   1
#define CANFD_FIFO_RX_BROADCAST 2
#define CANFD_FIFO_TX           3

#define CANFD_FIFO_DEPTH        8
#define CANFD_OBJ_SIZE          16
#define CANFD_RAM_SIZE          (3 * CANFD_FIFO_DEPTH * CANFD_OBJ_SIZE)
#define CANFD_RAM_ADDR          0x3800      // CAN message RAM 0x3800-0x3FFF (CxFIFOBA must be >= 0x3800), not used by the compiler

// FIFO n register block: CxFIFOCONn (+0..3), CxFIFOSTAn (+4..7), CxFIFOUAn (+8..11)
#define CANFD_FIFO_REG(n, off)  (*(&C1FIFOCON1L + 12u * ((n) - 1u) + (off)))
#define CANFD_FIFOCON_L         0
#define CANFD_FIFOCON_H         1           // UINC 0x01, TXREQ 0x02, FRESET 0x04
#define CANFD_FIFOCON_T         3           // PLSIZE[7:5], FSIZE[4:0]
#define CANFD_FIFOSTA_L         4           // TFNRFNIF 0x01, RXOVIF 0x08
#define CANFD_FIFOUA_L          8
#define CANFD_FIFOUA_H          9

#define CANFD_FIFOCON_UINC      0x01
#define CANFD_FIFOCON_TXREQ     0x02
#define CANFD_FIFOCON_FRESET    0x04
#define CANFD_FIFOSTA_TFNRFNIF  0x01        // RX: not empty, TX: not full
#define CANFD_FIFOSTA_RXOVIF    0x08

#define CANFD_EXIDE             0x40        // byte 3 of filter object (EXIDE) / mask (MIDE)

#define canfd_ram               ((volatile uint8_t *)CANFD_RAM_ADDR)

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
             | ((ecan_id[1] & 0x08) ? CANFD_EXIDE : 0);
}

// filter n (0-11) routed to fifo, enabled
static void canfd_set_filter(uint8_t n, uint8_t fifo, const uint8_t *ecan_id, const uint8_t *ecan_mask) {
    volatile uint8_t *fltcon = &C1FLTCON0L + n;
    volatile uint8_t *obj = &C1FLTOBJ0L + 8u * n;   // CxFLTOBJn followed by CxMASKn

    *fltcon = 0;                    // filter object can be changed only when the filter is disabled
    canfd_id(ecan_id, obj);
    canfd_id(ecan_mask, obj + 4);
    *fltcon = 0x80 | fifo;          // FLTEN | FnBP
}

/*
 * Pins (RB2 CANTX, RB3 CANRX), clock (FOSC), bit timing and FIFOs. Leaves the module in Configuration mode,
 * filters are set by the caller.
 */
static void canfd_init(void) {
    LATBbits.LATB2 = 1;             // recessive until the module drives the pin
    RB2PPS = 0x46;                  // CANTX
    CANRXPPS = 0x0B;                // RB3
    ANSELBbits.ANSELB3 = 0;
    TRISBbits.TRISB3 = 1;
    TRISBbits.TRISB2 = 0;

    C1CONHbits.ON = 1;
    canfd_set_mode(CANFD_MODE_CONFIG);

    C1CONL = 0x00;                  // CLKSEL0 = 0: FOSC, DNCNT 0
    C1CONH = 0x90;                  // ON, BRSDIS
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

    C1FIFOBAL = (uint8_t)CANFD_RAM_ADDR;
    C1FIFOBAH = (uint8_t)(CANFD_RAM_ADDR >> 8);
    C1FIFOBAU = 0;
    C1FIFOBAT = 0;

    // PLSIZE = 0 (8 B), FSIZE = depth - 1
    CANFD_FIFO_REG(CANFD_FIFO_RX_UNICAST, CANFD_FIFOCON_L) = 0x01;     // RX, TFNRFNIE
    CANFD_FIFO_REG(CANFD_FIFO_RX_UNICAST, CANFD_FIFOCON_T) = CANFD_FIFO_DEPTH - 1;
    CANFD_FIFO_REG(CANFD_FIFO_RX_BROADCAST, CANFD_FIFOCON_L) = 0x01;   // RX, TFNRFNIE
    CANFD_FIFO_REG(CANFD_FIFO_RX_BROADCAST, CANFD_FIFOCON_T) = CANFD_FIFO_DEPTH - 1;
    CANFD_FIFO_REG(CANFD_FIFO_TX, CANFD_FIFOCON_L) = 0x80;             // TXEN
    CANFD_FIFO_REG(CANFD_FIFO_TX, CANFD_FIFOCON_T) = CANFD_FIFO_DEPTH - 1;

    // filters are undefined after reset, disable all
    for (uint8_t i = 0; i < 12; ++i)
        *(&C1FLTCON0L + i) = 0;
}

static volatile uint8_t *canfd_fifo_obj(uint8_t fifo) {
    uint16_t ua = (uint16_t)((uint16_t)CANFD_FIFO_REG(fifo, CANFD_FIFOUA_H) << 8) | CANFD_FIFO_REG(fifo, CANFD_FIFOUA_L);
    if (ua >= CANFD_RAM_ADDR)       // absolute RAM address (FIFOBA + offset)
        ua -= CANFD_RAM_ADDR;
    return &canfd_ram[ua];
}

// RX FIFO not empty
static uint8_t canfd_rx_pending(uint8_t fifo) {
    return CANFD_FIFO_REG(fifo, CANFD_FIFOSTA_L) & CANFD_FIFOSTA_TFNRFNIF;
}

/*
 * Reads the head of an RX FIFO (canfd_rx_pending() must be true) into ECAN layout:
 * ecan_id[0..3] = SIDH, SIDL, EIDH, EIDL, returns DLC.
 */
static uint8_t canfd_rx_read(uint8_t fifo, uint8_t *ecan_id, uint8_t *data) {
    volatile uint8_t *obj = canfd_fifo_obj(fifo);

    uint8_t dlc = obj[4] & 0x0f;   // R1[3:0] (R1[15:11] = FILHIT)

    ecan_id[0] = (uint8_t)((obj[1] & 0x07) << 5) | (obj[0] >> 3);           // SID[10:3]
    ecan_id[1] = (uint8_t)(obj[0] << 5) | 0x08 | ((obj[3] >> 3) & 0x03);    // SID[2:0], EXIDE, EID[17:16]
    ecan_id[2] = (uint8_t)((obj[3] & 0x07) << 5) | (obj[2] >> 3);           // EID[15:8]
    ecan_id[3] = (uint8_t)((obj[2] & 0x07) << 5) | (obj[1] >> 3);           // EID[7:0]
    for (uint8_t i = 0; i < 8; ++i)
        data[i] = obj[8 + i];

    CANFD_FIFO_REG(fifo, CANFD_FIFOCON_H) |= CANFD_FIFOCON_UINC;
    return dlc;
}

// TX FIFO not full
static uint8_t canfd_tx_ready(void) {
    return CANFD_FIFO_REG(CANFD_FIFO_TX, CANFD_FIFOSTA_L) & CANFD_FIFOSTA_TFNRFNIF;
}

// Queues an extended frame, canfd_tx_ready() must be true
static void canfd_tx_write(const uint8_t *ecan_id, uint8_t dlc, const uint8_t *data) {
    volatile uint8_t *obj = canfd_fifo_obj(CANFD_FIFO_TX);

    canfd_id(ecan_id, obj);
    obj[3] &= 0x1f;                 // EID[17:13] only (SID11 = 0)
    obj[4] = 0x10 | (dlc & 0x0f);   // IDE, CAN 2.0 frame
    obj[5] = 0;
    obj[6] = 0;
    obj[7] = 0;
    for (uint8_t i = 0; i < 8; ++i)
        obj[8 + i] = data[i];

    CANFD_FIFO_REG(CANFD_FIFO_TX, CANFD_FIFOCON_H) |= CANFD_FIFOCON_UINC | CANFD_FIFOCON_TXREQ;
}

// Drops all frames waiting in the TX FIFO
static void canfd_tx_flush(void) {
    CANFD_FIFO_REG(CANFD_FIFO_TX, CANFD_FIFOCON_H) &= (uint8_t)~CANFD_FIFOCON_TXREQ;
    for (uint16_t t = 0xffff; t && (CANFD_FIFO_REG(CANFD_FIFO_TX, CANFD_FIFOCON_H) & CANFD_FIFOCON_TXREQ); --t);
    CANFD_FIFO_REG(CANFD_FIFO_TX, CANFD_FIFOCON_H) |= CANFD_FIFOCON_FRESET;
    for (uint16_t t = 0xffff; t && (CANFD_FIFO_REG(CANFD_FIFO_TX, CANFD_FIFOCON_H) & CANFD_FIFOCON_FRESET); --t);
}

#endif /* H9PIC_CANFD_H */
