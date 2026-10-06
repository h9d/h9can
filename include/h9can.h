// SPDX-License-Identifier: MIT
/*
 * H9 CAN protocol - node library API (AVR and PIC)
 *
 * Protocol: doc/protocol.md, standard registers: doc/standard_registers.md,
 * integration: doc/library.md.
 *
 * Copyright (C) 2024 Kamil Pałkowski
 *
 */

#ifndef H9CAN_H
#define H9CAN_H

#if defined (__AVR_ATmega16M1__) || defined (__AVR_ATmega32M1__) || defined (__AVR_ATmega64M1__) || defined (__AVR_AT90CAN128__) || defined (__AVR_ATmega32C1__)
#  include "h9avr/can.h"
#elif defined(__XC8)
#  include "h9pic/can.h"
#else
#error Unsupported MCU
#endif

#include "h9frame.h"
#include "h9def.h"

/**
 * @brief Initialise the CAN controller and the node.
 *
 * Call once at start, before enabling interrupts. It:
 * - reads the node ID from EEPROM (repairing a damaged copy first); without a stored ID
 *   @p default_id is used,
 * - stores the node metadata answered in the standard registers and NODE_INFO,
 * - sets the node flags: reset reason, bootloader presence / mismatch (bootloader info block
 *   compared with @p node_type, @p pcb_rev, @p bom_rev), default ID,
 * - configures 125 kbit/s and the receive filters (unicast to the node ID, DISCOVER /
 *   GROUP_RESET to the node type and to all nodes).
 *
 * AVR: F_CPU must be 4, 12 or 16 MHz. PIC: 16 MHz.
 *
 * @param node_type       Node type (16-bit, see doc/nodes.md), also the broadcast group of the node.
 * @param default_id      Node ID used when none is stored in EEPROM (1-254).
 * @param pcb_rev         PCB revision, ASCII letter ('A', 'B', ...).
 * @param bom_rev         BOM revision.
 * @param version_major   Firmware version major (0-1023 in NODE_INFO).
 * @param version_minor   Firmware version minor (0-2047 in NODE_INFO).
 * @param version_patch   Firmware version patch (0-2047 in NODE_INFO).
 * @param build_info      Null-terminated build info string (e.g. git describe), up to 31 characters are reported.
 * @retval 1              Node ID loaded from EEPROM.
 * @retval 0              No node ID in EEPROM, @p default_id is used (NODE_FLAG_DEFAULT_ID set).
 */
uint8_t CAN_init(uint16_t node_type, uint8_t default_id,
                 uint8_t pcb_rev, uint8_t bom_rev,
                 uint16_t version_major, uint16_t version_minor, uint16_t version_patch,
                 const char *build_info);

/**
 * @brief Check the CAN error state.
 *
 * @retval 1  An error counter reached the warning limit (96), or the controller is error
 *            passive / bus off.
 * @retval 0  Error active, counters below the warning limit.
 */
uint8_t CAN_bus_error_warning(void);

/**
 * @brief Broadcast NODE_TURNED_ON.
 *
 * Call once after CAN_init() and enabling interrupts. Payload: firmware version, PCB / BOM
 * revision and node flags (same as NODE_INFO), the group is the node type.
 */
void CAN_send_turned_on_broadcast(void);

/**
 * @brief Configure optional receive filter 1.
 *
 * Receive broadcast frames (types 16-31) of the given group, i.e. sent by nodes of the given
 * node type, from any source node. They are returned by CAN_get_msg() with value 2.
 * AVR: MOb 4. PIC: RXF4 (the controller is switched to configuration mode for the change).
 *
 * @param broadcast_group  Node type (broadcast group) to receive.
 */
void CAN_set_msg_filter_1(uint16_t broadcast_group);

/**
 * @brief Configure optional receive filter 2.
 *
 * Same as CAN_set_msg_filter_1(). AVR: MOb 5. PIC: RXF5.
 *
 * @param broadcast_group  Node type (broadcast group) to receive.
 */
void CAN_set_msg_filter_2(uint16_t broadcast_group);

#if !defined(__XC8)
/**
 * @brief Transmit a frame directly via MOb 0, without queuing (AVR only).
 *
 * Sets source_id to the node ID and, for broadcasts, the group to the node type.
 *
 * @param cm  Frame to transmit.
 * @retval 1  Loaded into MOb 0, transmission started.
 * @retval 0  MOb 0 busy, frame not sent.
 */
uint8_t CAN_try_put_msg(h9frame_t *cm);
#endif

/**
 * @brief Transmit a frame.
 *
 * Sets source_id to the node ID and, for broadcasts, the group to the node type; for unicast
 * frames the caller sets destination_id, seqnum and flags. Safe to call from interrupts.
 *
 * AVR: sent directly or queued (8 frames), never blocks. If the queue is full while the
 * controller is error passive / bus off, the newest queued frame is replaced by NODE_FAULT /
 * CAN_FRAME_LOSS and NODE_FLAG_CAN_TX_FRAME_LOSS is set.
 * PIC: waits for a free TX buffer (interrupts stay enabled while waiting); if all three are
 * busy while error passive / bus off, TXB2 is replaced by NODE_FAULT / CAN_FRAME_LOSS.
 *
 * @param cm  Frame to transmit.
 * @retval 1  Sent (AVR: loaded into MOb 0; PIC: loaded into a TX buffer).
 * @retval 2  Queued in the TX queue (AVR only).
 * @retval 0  Not sent (queue / buffers full).
 */
uint8_t CAN_put_msg(h9frame_t *cm);

/**
 * @brief Send COMMAND_ERROR as the answer to a request.
 *
 * @param errno        Error code, H9FRAME_ERROR_* (h9def.h).
 * @param destination  Node ID of the requester (request source_id).
 * @param seqnum       seqnum of the request.
 */
void send_command_error(uint8_t errno, uint8_t destination, uint8_t seqnum);

/**
 * @brief Broadcast NODE_FAULT.
 *
 * @param errno  Fault code, NODE_FAULT_* (h9def.h); node specific codes start at
 *               NODE_FAULT_NODE_SPECIFIC_FIRST_FAULT.
 */
void send_node_fault(uint8_t errno);

/**
 * @brief Receive and process the next frame (non-blocking).
 *
 * Call often from the main loop. Takes one frame from the RX buffer (16 frames) and handles
 * the protocol: standard registers 0-9, DISCOVER (answers NODE_INFO), NODE_RESET /
 * GROUP_RESET (restart), NODE_UPGRADE (jump to the bootloader), rejection of bootloader
 * frames. Before that it reports a receive buffer overflow (NODE_FAULT / CAN_RX_FRAME_LOSS)
 * and samples the CAN error state for the node flags.
 *
 * @param[out] cm  The received frame when returning 1 or 2.
 * @retval 1       Unicast frame for the application: GET_REG / SET_REG / SET_BIT / CLEAR_BIT
 *                 of a register >= 10 (must be answered), or REG_VALUE / COMMAND_ERROR from
 *                 another node.
 * @retval 2       Broadcast frame for the application (not DISCOVER / GROUP_RESET).
 * @retval 0       Nothing received, or the frame was handled by the library / not for this node.
 */
uint8_t CAN_get_msg(h9frame_t *cm);

/**
 * @brief Send a register value (REG_VALUE) as the answer to a request.
 *
 * Values up to 7 bytes are sent in one frame, longer ones (up to H9FRAME_MAX_REGISTER_SIZE)
 * as a multi-frame REG_VALUE (see doc/protocol.md).
 *
 * @param registry     Register number.
 * @param destination  Node ID of the requester (request source_id).
 * @param seqnum       seqnum of the request.
 * @param value        Register value, big-endian (most significant byte first).
 * @param length       Value length in bytes.
 */
void CAN_send_reg_value(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t *value, size_t length);

#endif //H9CAN_H
