// SPDX-License-Identifier: MIT
/*
 * H9 CAN protocol
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

/** Current node ID, loaded from EEPROM by CAN_init(). */
extern volatile uint8_t can_node_id;

/**
 * @brief Initialise the CAN peripheral and load the node ID from EEPROM.
 *
 * Must be called before sei(). Configures baud rate registers for the given
 * F_CPU, sets up receive MObs (unicast, broadcast-all, broadcast-by-type),
 * and stores the node metadata used when responding to standard registers.
 *
 * @param node_type       Application-specific node type (16-bit, see doc/nodes.md).
 * @param default_id      Default node id
 * @param version_major   Firmware version major number.
 * @param version_minor   Firmware version minor number.
 * @param version_patch   Firmware version patch number.
 * @param build_info      Null-terminated build-info string (e.g. git-describe output).
 */
void CAN_init(uint16_t node_type,  uint8_t default_id,
              uint16_t version_major, uint16_t version_minor, uint16_t version_patch,
              const char *build_info);

/**
 * @brief Broadcast a NODE_TURNED_ON message onto the bus.
 *
 * Call once after sei() and a short stabilisation delay. The frame carries
 * node type, firmware version, hardware revision and the last reset reason.
 */
void CAN_send_turned_on_broadcast(void);

/**
 * @brief Configure optional receive filter 1 (hardware MOb 4).
 *
 * Enables reception of messages from a specific remote node and/or broadcast
 * messages for a specific node-type group. Pass @c active = 0 to disable the
 * corresponding filter field (accept any value).
 *
 * @param remote_node_id          Source node ID to match.
 * @param remote_node_id_active   1 to apply @p remote_node_id filter, 0 to ignore.
 * @param broadcast_group         Broadcast node-type group to match.
 * @param broadcast_group_active  1 to apply @p broadcast_group filter, 0 to ignore.
 */
void CAN_set_msg_filter_1(uint8_t remote_node_id, uint8_t remote_node_id_active,
                           uint16_t broadcast_group, uint8_t broadcast_group_active);

/**
 * @brief Configure optional receive filter 2 (hardware MOb 5).
 *
 * Identical semantics to CAN_set_msg_filter_1().
 *
 * @param remote_node_id          Source node ID to match.
 * @param remote_node_id_active   1 to apply @p remote_node_id filter, 0 to ignore.
 * @param broadcast_group         Broadcast node-type group to match.
 * @param broadcast_group_active  1 to apply @p broadcast_group filter, 0 to ignore.
 */
void CAN_set_msg_filter_2(uint8_t remote_node_id, uint8_t remote_node_id_active,
                           uint16_t broadcast_group, uint8_t broadcast_group_active);

/**
 * @brief Attempt to transmit a message directly via MOb 0, without buffering.
 *
 * Does not queue the message if the MOb is busy.
 *
 * @param cm  Message to transmit.
 * @retval 1  Message loaded into MOb 0 and transmission started.
 * @retval 0  MOb 0 is busy; message was not sent.
 */
uint8_t CAN_try_put_msg(h9frame_t *cm);

/**
 * @brief Transmit a message, queuing it if the hardware MOb is busy.
 *
 * Attempts direct transmission via CAN_try_put_msg(). If the MOb is occupied,
 * the frame is placed in the 8-entry TX ring buffer and sent from the TX-complete
 * interrupt.
 *
 * @param cm  Message to transmit.
 * @retval 1  Sent immediately.
 * @retval 2  Queued in the TX ring buffer.
 * @retval 0  TX buffer full; message dropped.
 */
uint8_t CAN_put_msg(h9frame_t *cm);

/**
 * @brief Send a COMMAND_ERROR response.
 *
 * @param errno        Error code (H9FRAME_ERROR_* from h9def.h).
 * @param destination  Destination node ID for the error response.
 * @param seqnum       Sequence number copied from the originating request.
 */
void send_command_error(uint8_t errno, uint8_t destination, uint8_t seqnum);

void send_node_fault(uint8_t errno);

/**
 * @brief Dispatch a received message — internal processing and application routing.
 *
 * Handles broadcast messages (DISCOVER, GROUP_RESET), unicast control messages
 * (NODE_RESET, NODE_UPGRADE), and register access (GET_REG, SET_REG, SET_BIT,
 * CLEAR_BIT). Standard registers (0–9) are handled internally; register numbers
 * ≥ 10 are left in @p cm for the application.
 *
 * @param cm  Received message. On return, may be overwritten if routed internally.
 * @retval 2  Message is broadcast.
 * @retval 1  Message is for the application (register ≥ 10, or REG_VALUE / COMMAND_ERROR).
 * @retval 0  Message was handled internally; application should ignore @p cm.
 */
uint8_t process_msg(h9frame_t *cm);

/**
 * @brief Receive and process the next pending CAN message (non-blocking).
 *
 * Dequeues one frame from the RX ring buffer and calls process_msg(). Standard
 * protocol messages are handled internally without involving the application.
 *
 * @param[out] cm  Populated with the received message when returning 1.
 * @retval 1       A message intended for the application is available in @p cm.
 * @retval 0       No message pending, or message was handled internally.
 */
uint8_t CAN_get_msg(h9frame_t *cm);

/**
 * @brief Initialise an outgoing message with default fields.
 *
 * Sets priority to LOW, fills in the current node ID as source, clears flags,
 * and sets dlc to 0. The sequence number is assigned automatically just before
 * transmission by CAN_try_put_msg().
 *
 * @param[out] mes  Message structure to initialise.
 */
void CAN_init_new_msg(h9frame_t *mes);

/**
 * @brief Initialise a response message from a received request.
 *
 * Copies priority and seqnum from @p req, sets source/destination appropriately,
 * and infers the response type:
 * - GET_REG / SET_REG / SET_BIT / CLEAR_BIT → REG_VALUE
 * - DISCOVER → NODE_INFO
 *
 * @param[in]  req  The received request message.
 * @param[out] res  Response message to initialise; dlc is set to 0.
 */
void CAN_init_response_msg(const h9frame_t *req, h9frame_t *res);

void CAN_send_reg_value(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t *value, size_t length);

#endif //H9CAN_H
