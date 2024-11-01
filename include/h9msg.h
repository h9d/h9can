// SPDX-License-Identifier: MIT
/*
 * H9 CAN message
 *
 * Copyright (C) 2017-2024 Kamil Pałkowski
 *
 */

#ifndef H9MSG_H
#define H9MSG_H

#include <stdint.h>

#define H9MSG_PRIORITY_BIT_LENGTH 1
#define H9MSG_TYPE_BIT_LENGTH 5
#define H9MSG_SEQNUM_BIT_LENGTH 5
#define H9MSG_ID_BIT_LENGTH 9

#define H9MSG_BROADCAST_ID 0x1ff

#define H9MSG_PRIORITY_HIGH 0
#define H9MSG_PRIORITY_LOW 1

#define H9MSG_MAX_REGISTER_SIZE 32

#define H9MSG_BOOTLOADER_MSG_GROUP 0
#define H9MSG_BOOTLOADER_MSG_GROUP_MASK 24

#define H9MSG_NODE_STANDARD_MSG_GROUP 8
#define H9MSG_NODE_STANDARD_MSG_GROUP_MASK 24
#define H9MSG_NODE_STANDARD_MSG_BROADCAST_SUBGROUP 14
#define H9MSG_NODE_STANDARD_MSG_BROADCAST_SUBGROUP_MASK 30

#define H9MSG_NODE_RESPONSE_MSG_GROUP 16
#define H9MSG_NODE_RESPONSE_MSG_GROUP_MASK 24
#define H9MSG_NODE_SPECIFIC_BULK_MSG_GROUP 24
#define H9MSG_NODE_SPECIFIC_BULK_MSG_GROUP_MASK 24
#define H9MSG_NODE_ALL_REMOTE_MSG_GROUP 16
#define H9MSG_NODE_ALL_REMOTE_MSG_GROUP_MASK 16


#define H9MSG_TYPE_NOP 0
#define H9MSG_TYPE_PAGE_START 1
#define H9MSG_TYPE_QUIT_BOOTLOADER 2
#define H9MSG_TYPE_PAGE_FILL 3
#define H9MSG_TYPE_BOOTLOADER_TURNED_ON 4
#define H9MSG_TYPE_PAGE_FILL_NEXT 5
#define H9MSG_TYPE_PAGE_WRITED 6
#define H9MSG_TYPE_PAGE_FILL_BREAK 7

#define H9MSG_TYPE_SET_REG 8
#define H9MSG_TYPE_GET_REG 9
#define H9MSG_TYPE_SET_BIT 10
#define H9MSG_TYPE_CLEAR_BIT 11
#define H9MSG_TYPE_TOGGLE_BIT 12
#define H9MSG_TYPE_NODE_UPGRADE 13
#define H9MSG_TYPE_NODE_RESET 14
#define H9MSG_TYPE_DISCOVER 15

#define H9MSG_TYPE_REG_EXTERNALLY_CHANGED 16
#define H9MSG_TYPE_REG_INTERNALLY_CHANGED 17
#define H9MSG_TYPE_REG_VALUE_BROADCAST 18
#define H9MSG_TYPE_REG_VALUE 19
#define H9MSG_TYPE_ERROR 20
#define H9MSG_TYPE_NODE_HEARTBEAT 21
#define H9MSG_TYPE_NODE_INFO 22
#define H9MSG_TYPE_NODE_TURNED_ON 23

#define H9MSG_TYPE_NODE_SPECIFIC_BULK0 24
#define H9MSG_TYPE_NODE_SPECIFIC_BULK1 25
#define H9MSG_TYPE_NODE_SPECIFIC_BULK2 26
#define H9MSG_TYPE_NODE_SPECIFIC_BULK3 27
#define H9MSG_TYPE_NODE_SPECIFIC_BULK4 28
#define H9MSG_TYPE_NODE_SPECIFIC_BULK5 29
#define H9MSG_TYPE_NODE_SPECIFIC_BULK6 30
#define H9MSG_TYPE_NODE_SPECIFIC_BULK7 31


// 31 30 29 | 28 27 26 25 24 23 22 21 | 20 19 18 17 16 15 14 13 | 12 11 10 09 08 07 06 05 | 04 03 02 01 00
// -- -- -- | pp ty ty ty ty ty se se | se se se ds ds ds ds ds | ds ds ds ds so so so so | so so so so so

#define H9MSG_ERROR_INVALID_MSG 1
#define H9MSG_ERROR_BOOTLOADER_UNSUPPORTED 2
#define H9MSG_ERROR_INVALID_REGISTER 3
#define H9MSG_ERROR_READ_ONLY_REGISTER 4
#define H9MSG_ERROR_WRITE_ONLY_REGISTER 5
#define H9MSG_ERROR_REGISTER_SIZE_MISMATCH 6
#define H9MSG_ERROR_NODE_SPECIFIC_ERROR 0xff

struct h9msg {
    uint8_t priority :H9MSG_PRIORITY_BIT_LENGTH;
    uint8_t type :H9MSG_TYPE_BIT_LENGTH;
    uint8_t seqnum: H9MSG_SEQNUM_BIT_LENGTH;
    uint16_t destination_id :H9MSG_ID_BIT_LENGTH;
    uint16_t source_id :H9MSG_ID_BIT_LENGTH;
    uint8_t dlc;
    uint8_t data[8];
};

typedef struct h9msg h9msg_t;

#endif /* H9MSG_H */
