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
#include "h9def.h"

// #define H9FRAME_TYPE_BIT_LENGTH 5
// #define H9FRAME_FLAGS_BITS_LENGTH  3
// #define H9FRAME_SEQNUM_BIT_LENGTH 5
// #define H9FRAME_ID_BIT_LENGTH 8
// #define H9FRAME_BROADCAST_GROUP_LENGTH (H9FRAME_FLAGS_BITS_LENGTH + H9FRAME_ID_BIT_LENGTH + H9FRAME_SEQNUM_BIT_LENGTH)

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

typedef struct h9frame h9frame_t;

#endif /* H9FRAME_H */
