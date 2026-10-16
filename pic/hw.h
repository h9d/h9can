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

// can.c, for the CAN RX interrupt: free RX buffer slot or NULL when the buffer is full (frame loss is reported)
can_buf_t *can_rx_slot(void);
void can_rx_commit(void);

/* --- CAN controller --- */

// Pins, bit timing (125 kbps), acceptance filters, RX interrupts (low priority), leaves the controller in Normal mode
void can_hw_init(uint8_t node_id, uint16_t node_type);
// n = 1, 2: broadcast group filters set by CAN_set_msg_filter_1/2
void can_hw_set_broadcast_filter(uint8_t n, uint16_t broadcast_group);
uint8_t can_hw_error_warning(void);
// bus passive / bus off
uint8_t can_hw_bus_error(void);
// hardware RX overflow, the flags are cleared
uint8_t can_hw_rx_overflow(void);
// Queues an extended frame, returns 0 when all TX buffers are busy. Interrupts must be disabled.
uint8_t can_hw_tx(const uint8_t *id, uint8_t dlc, const uint8_t *data);
// Drops pending frame(s) and queues this one, it is sent automatically after bus-off recovery. Interrupts must be disabled.
void can_hw_tx_on_bus_error(const uint8_t *id, uint8_t dlc, const uint8_t *data);

/* --- MCU --- */

// NODE_RESET_* (h9def.h), clears the reset flags
uint8_t mcu_reset_reason(void);
// Disables interrupts (GIEH, GIEL), returns the previous state for mcu_irq_restore()
uint8_t mcu_irq_disable(void);
void mcu_irq_restore(uint8_t state);

#endif /* H9PIC_HW_H */
