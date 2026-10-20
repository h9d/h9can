/*
 * MCU family specific part of the PIC library (private to h9can/pic).
 *
 * Implemented in pic/18F46K80 (ECAN) and pic/18FXXQ84 (CAN FD module used in CAN 2.0 mode). Every file there
 * compiles only for its own family, so a project may add both directories.
 */

#ifndef H9PIC_HW_H
#define H9PIC_HW_H

#include <xc.h>
#include <stdint.h>
#include <stddef.h>

#if !defined(_18F66K80_FAMILY_) && !defined(_18FXXQ84_FAMILY_)
#error "Unsupported MCU family, supported: PIC18F46K80, PIC18F47Q84"
#endif

// Received frame, id in ECAN layout: SIDH, SIDL, EIDH, EIDL (calc_can_*_id, h9pic/common.h)
typedef struct {
    uint8_t id[4];
    uint8_t dlc;
    uint8_t data[8];
} can_buf_t;

/* --- CAN controller --- */

// Pins, bit timing (125 kbps), acceptance filters (K80: RX interrupts, low priority), leaves the controller in Normal mode
void can_hw_init(uint8_t node_id, uint16_t node_type);
// n = 1, 2: broadcast group filters set by CAN_set_msg_filter_1/2
void can_hw_set_broadcast_filter(uint8_t n, uint16_t broadcast_group);
uint8_t can_hw_error_warning(void);
// bus passive / bus off
uint8_t can_hw_bus_error(void);
// Next received frame (unicast first), returns 0 when there is none
uint8_t can_hw_rx(can_buf_t *frame);
// RX frame lost since the last call (hardware overflow, K80 also its software buffer), the flags are cleared
uint8_t can_hw_rx_overflow(void);
// Queues an extended frame, returns 0 when the TX buffers are busy. Interrupts must be disabled.
// urgent (NODE_FAULT): Q84 sends it through the TXQ, ahead of the normal frames waiting in FIFO 3; K80 ignores it.
uint8_t can_hw_tx(const uint8_t *id, uint8_t dlc, const uint8_t *data, uint8_t urgent);
// Bus error with the TX buffers full: queues this fault frame, it is sent automatically after bus-off recovery
// (K80: replaces the frame waiting in TXB2; Q84: TXQ, only if no fault is waiting there). Interrupts must be disabled.
void can_hw_tx_on_bus_error(const uint8_t *id, uint8_t dlc, const uint8_t *data);

/* --- MCU --- */

// NODE_RESET_* (h9def.h), clears the reset flags
uint8_t mcu_reset_reason(void);
// Disables interrupts (GIEH, GIEL), returns the previous state for mcu_irq_restore()
uint8_t mcu_irq_disable(void);
void mcu_irq_restore(uint8_t state);

#endif /* H9PIC_HW_H */
