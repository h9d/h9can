/*
 * h9pic-bootloader
 *
 * Created by SQ8KFH on 2020-07-29.
 *
 * Copyright (C) 2020 Kamil Palkowski. All rights reserved.
 */


#include "config.h"
#include <xc.h>
#include "can.h"
#include <h9pic/bl_info.h>

#include "version.h"

// version packed into 32 bits: major (10 bits) | minor (11 bits) | patch (11 bits)
#if VERSION_MAJOR > 0x3ff || VERSION_MINOR > 0x7ff || VERSION_PATCH > 0x7ff
#error "Bootloader version out of range (major 0-1023, minor/patch 0-2047)"
#endif
#define BOOTLOADER_VERSION_PACKED (((uint32_t)VERSION_MAJOR << 22) | ((uint32_t)VERSION_MINOR << 11) | (uint32_t)VERSION_PATCH)

// info block at the end of flash, read by the application (see h9pic/bl_info.h)
const h9_bl_info_t bl_info __at(H9_BL_INFO_ADDR) = {
    .magic     = H9_BL_INFO_MAGIC,
    .node_type = NODE_TYPE,
    .pcb_rev   = PCB_REVISION,
    .bom_rev   = BOM_REVISION,
    .version   = BOOTLOADER_VERSION_PACKED,
};

#define FLASH_BLOCK_SIZE 64

static uint8_t seqnum = 0;

void StartWrite(void) {
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1;       // Start the write
    NOP();
    NOP();
}

void erase_block(uint24_t tblptr) {
    TBLPTR = tblptr;
    EECON1 = 0x94;       // Setup writes
    StartWrite();
}

// must match the code offset of the MPLAB project (bootloader location)
#define BOOTLOADER_START 0xF600
// blocks below the bootloader
#define APP_BLOCKS (BOOTLOADER_START / FLASH_BLOCK_SIZE)

// host id is locked at the first PAGE_START, frames from other nodes are ignored since then
#define HOST_ID_NONE 0x100

// receive timeouts (about 1-2 s each) in a row before the block is abandoned
#define PAGE_FILL_RETRIES 4

static void write_block(uint16_t block, uint8_t host_id) {
    uint8_t bytes_remain = FLASH_BLOCK_SIZE;
    uint24_t tblptr = (uint24_t)block * FLASH_BLOCK_SIZE;

    h9frame_t cm_res;
    cm_res.unicast.destination_id = host_id;
    cm_res.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;

    erase_block(tblptr);

    TBLPTR = tblptr;
    EECON1 = 0x84;       // Setup writes
    while (1) {
        // every new PAGE_FILL_NEXT gets the next seqnum, the host copies it to PAGE_FILL
        cm_res.type = H9FRAME_TYPE_PAGE_FILL_NEXT;
        cm_res.unicast.seqnum = ++seqnum;
        cm_res.dlc = 2;
        cm_res.data[0] = 0;
        cm_res.data[1] = bytes_remain;
        CAN_put_msg_blocking(&cm_res);

        h9frame_t cm;
        uint8_t retries = PAGE_FILL_RETRIES;
        while (1) {
            if (!CAN_get_msg_blocking(&cm)) {
                // PAGE_FILL or the response was lost - repeat the response (the same seqnum)
                if (--retries == 0)
                    goto block_break;
                CAN_put_msg_blocking(&cm_res);
                continue;
            }
            if (cm.source_id != host_id)
                continue;
            // any other bootloader frame from the host aborts the block
            if (cm.type != H9FRAME_TYPE_PAGE_FILL)
                goto block_break;
            // PAGE_FILL answering an earlier (repeated) response is a duplicate, ignored
            if (cm.dlc == 8 && cm.unicast.seqnum == cm_res.unicast.seqnum)
                break;
        }

        for (uint8_t i = 0; i < 8; ++i) {
            TABLAT = cm.data[i];
            asm("TBLWT *+");
        }
        bytes_remain -= 8;

        if (bytes_remain == 0) {
            asm("TBLRD *-");
            StartWrite();
            EECON1bits.WREN = 0;

            cm_res.type = H9FRAME_TYPE_PAGE_WRITED;
            cm_res.data[0] = (block >> 8) & 0xff;
            cm_res.data[1] = (block) & 0xff;
            CAN_put_msg_blocking(&cm_res);
            return;
        }
    }

block_break:
    EECON1bits.WREN = 0;
    cm_res.type = H9FRAME_TYPE_PAGE_FILL_BREAK;
    cm_res.dlc = 0;
    CAN_put_msg_blocking(&cm_res);
}

void main(void) {
    INTCONbits.PEIE = 0;
    INTCONbits.GIE = 0;
    CAN_init();
    
    h9frame_t turn_on_msg;

    turn_on_msg.type = H9FRAME_TYPE_BOOTLOADER_TURNED_ON;
    turn_on_msg.broadcast.group = NODE_TYPE;
    turn_on_msg.dlc = 8;

    // version big-endian
    const uint32_t version = BOOTLOADER_VERSION_PACKED;
    turn_on_msg.data[0] = (version >> 24) & 0xff;
    turn_on_msg.data[1] = (version >> 16) & 0xff;
    turn_on_msg.data[2] = (version >> 8) & 0xff;
    turn_on_msg.data[3] = version & 0xff;

    turn_on_msg.data[4] = PCB_REVISION;
    turn_on_msg.data[5] = BOM_REVISION;
    turn_on_msg.data[6] = NODE_MCU_PIC18F46K80;
#if _XTAL_FREQ == 16000000
    turn_on_msg.data[7] = NODE_MCU_F_16MHz;
#else
#error "Unknow node MCU_F"
#endif
    
    CAN_put_msg_blocking(&turn_on_msg);
    
    uint16_t host_id = HOST_ID_NONE;
    h9frame_t cm;
    while (1) {
        if (CAN_get_msg_blocking(&cm)) {
            if (host_id != HOST_ID_NONE && cm.source_id != host_id)
                continue;

            if (cm.type == H9FRAME_TYPE_PAGE_START && cm.dlc == 2) {
                uint16_t block = (uint16_t)(cm.data[0] << 8) | cm.data[1];
                if (block < APP_BLOCKS) {
                    host_id = cm.source_id;
                    write_block(block, (uint8_t)host_id);
                }
            }
            else if (cm.type == H9FRAME_TYPE_QUIT_BOOTLOADER && cm.dlc == 0) {
                STKPTR = 0x00;
                RESET();
                //asm  ("goto 0x0000");
            }
        }
        else {
            CAN_put_msg_blocking(&turn_on_msg);
        }
    }
    return;
}
