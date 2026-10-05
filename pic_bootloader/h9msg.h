/*
 * h9pic-bootloader
 *
 * Created by SQ8KFH on 2020-07-29.
 *
 * Copyright (C) 2020 Kamil Palkowski. All rights reserved.
 */

#ifndef H9MSG_H
#define H9MSG_H

#include <stdint.h>

#define H9MSG_PRIORITY_BIT_LENGTH 1
#define H9MSG_TYPE_BIT_LENGTH 5
#define H9MSG_SEQNUM_BIT_LENGTH 5
#define H9MSG_DESTINATION_ID_BIT_LENGTH 9
#define H9MSG_SOURCE_ID_BIT_LENGTH 9

#define H9MSG_BROADCAST_ID 0x1ff

#define H9MSG_PRIORITY_HIGH 0
#define H9MSG_PRIORITY_LOW 1

#define H9MSG_TYPE_GROUP_MASK 24
#define H9MSG_TYPE_DOUBLE_GROUP_MASK 16
#define H9MSG_TYPE_SUBGROUP_MASK 28

/* 00.... BOOTLOADER + NOP*/
#define H9MSG_TYPE_GROUP_0 0

#define H9MSG_TYPE_NOP 0
#define H9MSG_TYPE_PAGE_START 1
#define H9MSG_TYPE_QUIT_BOOTLOADER 2
#define H9MSG_TYPE_PAGE_FILL 3
#define H9MSG_TYPE_BOOTLOADER_TURNED_ON 4
#define H9MSG_TYPE_PAGE_FILL_NEXT 5
#define H9MSG_TYPE_PAGE_WRITED 6
#define H9MSG_TYPE_PAGE_FILL_BREAK 7


// 31 30 29 | 28 27 26 25 24 23 22 21 | 20 19 18 17 16 15 14 13 | 12 11 10 09 08 07 06 05 | 04 03 02 01 00
// -- -- -- | pp ty ty ty ty ty se se | se se se ds ds ds ds ds | ds ds ds ds so so so so | so so so so so

struct h9msg {
    uint8_t priority :H9MSG_PRIORITY_BIT_LENGTH;
    uint8_t type :H9MSG_TYPE_BIT_LENGTH;
    uint8_t seqnum: H9MSG_SEQNUM_BIT_LENGTH;
    uint16_t destination_id;
    uint16_t source_id;
    uint8_t dlc;
    uint8_t data[8];
};

typedef struct h9msg h9msg_t;

#endif /* H9MSG_H */
