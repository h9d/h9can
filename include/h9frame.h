// SPDX-License-Identifier: MIT
/*
 * H9 CAN frame
 *
 * Copyright (C) 2017-2026 Kamil Pałkowski
 *
 */

#ifndef H9FRAME_H
#define H9FRAME_H

#include <stdint.h>

#if defined(__cplusplus)
#define H9FRAME_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define H9FRAME_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define H9FRAME_STATIC_ASSERT(cond, msg) typedef char h9frame_static_assert_failed[(cond) ? 1 : -1]
#endif

#define H9FRAME_TYPE_BIT_LENGTH 5
#define H9FRAME_FLAGS_BITS_LENGTH  3
#define H9FRAME_SEQNUM_BIT_LENGTH 5
#define H9FRAME_ID_BIT_LENGTH 8
#define H9FRAME_BROADCAST_GROUP_LENGTH (H9FRAME_FLAGS_BITS_LENGTH + H9FRAME_ID_BIT_LENGTH + H9FRAME_SEQNUM_BIT_LENGTH)

#define H9FRAME_FLAG_SINGE_MSG 0
#define H9FRAME_FLAG_MULTI_MSG_FIRST 1
#define H9FRAME_FLAG_MULTI_MSG_MIDDLE 2
#define H9FRAME_FLAG_MULTI_MSG_LAST 3

#define H9FRAME_MAX_REGISTER_SIZE 32

#define H9FRAME_BOOTLOADER_MSG_TYPE_GROUP 0
#define H9FRAME_BOOTLOADER_MSG_TYPE_GROUP_MASK 24

#define H9FRAME_UNICAST_MSG_TYPE_GROUP 8
#define H9FRAME_UNICAST_MSG_TYPE_GROUP_MASK 24

#define H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP 16
#define H9FRAME_ALL_BROADCAST_MSG_TYPE_GROUP_MASK 24

#define H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP 16
#define H9FRAME_SPECIAL_BROADCAST_MSG_TYPE_GROUP_MASK 30

#define H9FRAME_ID_MASK ((1 << H9FRAME_ID_BIT_LENGTH) - 1)
#define H9FRAME_NODE_TYPE_MASK ((1UL << H9FRAME_BROADCAST_GROUP_LENGTH) - 1UL)

#define H9FRAME_UNICAST_BROADCAST_BIT 16
#define H9FRAME_BROADCAST_ID ((1UL << H9FRAME_BROADCAST_GROUP_LENGTH) - 1UL)

#define H9FRAME_NODE_RESPONSE_MSG_GROUP 16
#define H9FRAME_NODE_RESPONSE_MSG_GROUP_MASK 24
#define H9FRAME_NODE_SPECIFIC_BULK_MSG_GROUP 24
#define H9FRAME_NODE_SPECIFIC_BULK_MSG_GROUP_MASK 24
#define H9FRAME_NODE_ALL_REMOTE_MSG_GROUP 16
#define H9FRAME_NODE_ALL_REMOTE_MSG_GROUP_MASK 16

enum {
    /* --- UNICAST --- */
    H9FRAME_TYPE_RES1 = 0,
    H9FRAME_TYPE_PAGE_START = 1,
    H9FRAME_TYPE_QUIT_BOOTLOADER = 2,
    H9FRAME_TYPE_PAGE_FILL = 3,
    H9FRAME_TYPE_RES2 = 4,
    H9FRAME_TYPE_PAGE_FILL_NEXT = 5,
    H9FRAME_TYPE_PAGE_WRITED = 6,
    H9FRAME_TYPE_PAGE_FILL_BREAK = 7,

    H9FRAME_TYPE_COMMAND_ERROR =  8,
    H9FRAME_TYPE_REG_VALUE =  9,
    H9FRAME_TYPE_SET_REG =  10,
    H9FRAME_TYPE_GET_REG =  11,
    H9FRAME_TYPE_SET_BIT =  12,
    H9FRAME_TYPE_CLEAR_BIT =  13,
    H9FRAME_TYPE_NODE_UPGRADE =  14,
    H9FRAME_TYPE_NODE_RESET =  15,

    /* --- SPECIAL BROADCAST --- */
    H9FRAME_TYPE_DISCOVER = 16,
    H9FRAME_TYPE_GROUP_RESET = 17,

    /* --- BROADCAST --- */
    H9FRAME_TYPE_NODE_FAULT = 18,
    H9FRAME_TYPE_REG_VALUE_BROADCAST = 19,
    H9FRAME_TYPE_NODE_HEARTBEAT = 20,
    H9FRAME_TYPE_NODE_INFO = 21,
    H9FRAME_TYPE_NODE_TURNED_ON = 22,
    H9FRAME_TYPE_BOOTLOADER_TURNED_ON = 23,

    H9FRAME_TYPE_NODE_SPECIFIC_BROADCAST0 = 24,
    H9FRAME_TYPE_NODE_SPECIFIC_BROADCAST1 = 25,
    H9FRAME_TYPE_NODE_SPECIFIC_BROADCAST2 = 26,
    H9FRAME_TYPE_NODE_SPECIFIC_BROADCAST3 = 27,
    H9FRAME_TYPE_NODE_SPECIFIC_BROADCAST4 = 28,
    H9FRAME_TYPE_NODE_SPECIFIC_BROADCAST5 = 29,
    H9FRAME_TYPE_NODE_SPECIFIC_BROADCAST6 = 30,
    H9FRAME_TYPE_NODE_SPECIFIC_BROADCAST7 = 31
};

//          | CANIDT1                     | CANIDT2                 | CANIDT3                 | CANIDT4
// 31 30 29 | 28     27 26 25 24 23 22 21 | 20 19 18 17 16 15 14 13 | 12 11 10 09 08 07 06 05 | 04 03 02 01 00 -- -- --
// -- -- -- | ty_(0) ty ty ty ty so so so | so so so so so fl fl fl | ds ds ds ds ds ds ds ds | sq sq sq sq sq -- -- --
// -- -- -- | ty_(1) ty ty ty ty so so so | so so so so so nt nt nt | nt nt nt nt nt nt nt nt | nt nt nt nt nt -- -- --

//          | SIDH                        | SIDL                    | EIDH                    | EIDL
// 31 30 29 | 28     27 26 25 24 23 22 21 | 20 19 18 ** ** ** 17 16 | 15 14 13 12 11 10 09 08 | 07 06 05 04 03 02 01 00
// -- -- -- | ty_(0) ty ty ty ty so so so | so so so **  1 ** so so | fl fl fl ds ds ds ds ds | ds ds ds sq sq sq sq sq
// -- -- -- | ty_(1) ty ty ty ty so so so | so so so **  1 ** so so | nt nt nt nt nt nt nt nt | nt nt nt nt nt nt nt nt
struct h9frame {
    uint8_t type: H9FRAME_TYPE_BIT_LENGTH;
    uint8_t source_id: H9FRAME_ID_BIT_LENGTH;

    union {
        struct {
            unsigned int seqnum : H9FRAME_SEQNUM_BIT_LENGTH;
            unsigned int destination_id : H9FRAME_ID_BIT_LENGTH;
            unsigned int flags : H9FRAME_FLAGS_BITS_LENGTH;
        } unicast;

        struct {
            uint16_t group;
        } broadcast;
    };

    uint8_t dlc;
    uint8_t data[8];
};

H9FRAME_STATIC_ASSERT(
    H9FRAME_TYPE_BIT_LENGTH + H9FRAME_ID_BIT_LENGTH + H9FRAME_FLAGS_BITS_LENGTH + H9FRAME_ID_BIT_LENGTH + H9FRAME_SEQNUM_BIT_LENGTH == 29, "CAN id must be 29-bits"
);

typedef struct h9frame h9frame_t;

#endif /* H9FRAME_H */
