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
#include <h9pic/mcu.h>

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

#if defined(_18FXXQ84_FAMILY_)
/*
 * Flash is erased in 256 B pages (128 words), the protocol writes 64 B blocks: the page is read to RAM,
 * the received block is put into it and the whole page is erased and written back.
 */
#define FLASH_PAGE_SIZE (_FLASH_ERASE_SIZE * 2)

static uint8_t page_buf[FLASH_PAGE_SIZE];

static void nvm_start(uint8_t cmd) {
    NVMCON1bits.CMD = cmd;
    NVMLOCK = 0x55;
    NVMLOCK = 0xAA;
    NVMCON0bits.GO = 1;
    while (NVMCON0bits.GO);
}

static void nvm_set_addr(uint24_t addr) {
    NVMADRU = (uint8_t)(addr >> 16);
    NVMADRH = (uint8_t)(addr >> 8);
    NVMADRL = (uint8_t)addr;
}

static void read_page(uint24_t page_addr) {
    TBLPTR = page_addr;
    for (uint16_t i = 0; i < FLASH_PAGE_SIZE; ++i) {
        asm("TBLRD *+");
        page_buf[i] = TABLAT;
    }
}

static void write_page(uint24_t page_addr) {
    if (page_addr >= H9PIC_BOOTLOADER_ADDR)     // never erase the bootloader itself
        return;

    nvm_set_addr(page_addr);
    nvm_start(0x06);                            // page erase

    for (uint16_t i = 0; i < FLASH_PAGE_SIZE; i += 2) {
        if (page_buf[i] == 0xff && page_buf[i + 1] == 0xff)
            continue;                           // erased already
        nvm_set_addr(page_addr + i);
        NVMDATL = page_buf[i];
        NVMDATH = page_buf[i + 1];
        nvm_start(0x03);                        // word write
    }
    NVMCON1bits.CMD = 0x00;
}

void write_block(uint16_t block, uint8_t dst_id) {
    uint16_t bytes_remain = FLASH_BLOCK_SIZE;
    uint24_t tblptr = (uint24_t)block * FLASH_BLOCK_SIZE;
    uint24_t page_addr = tblptr & ~(uint24_t)(FLASH_PAGE_SIZE - 1);
    uint8_t *dst = &page_buf[(uint8_t)(tblptr - page_addr)];

    read_page(page_addr);

    while (1) {
        h9frame_t cm;
        CAN_get_msg_blocking(&cm);

        h9frame_t cm_res;
        cm_res.unicast.seqnum = seqnum++;
        cm_res.unicast.destination_id = dst_id;
        cm_res.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;

        if (cm.source_id == dst_id && cm.type == H9FRAME_TYPE_PAGE_FILL && cm.dlc == 8) {
            for (uint8_t i = 0; i < cm.dlc; ++i) {
                *dst++ = cm.data[i];
                --bytes_remain;
            }

            if (bytes_remain == 0) {
                cm_res.type = H9FRAME_TYPE_PAGE_WRITED;
                cm_res.dlc = 2;
                cm_res.data[0] = (block >> 8) & 0xff;
                cm_res.data[1] = (block) & 0xff;

                write_page(page_addr);
                CAN_put_msg_blocking(&cm_res);
                break;
            }
            else {
                cm_res.type = H9FRAME_TYPE_PAGE_FILL_NEXT;
                cm_res.dlc = 2;
                cm_res.data[0] = (bytes_remain >> 8) & 0xff;
                cm_res.data[1] = (bytes_remain) & 0xff;
                CAN_put_msg_blocking(&cm_res);
            }
        }
        else if (cm.source_id == dst_id && (cm.type & H9FRAME_BOOTLOADER_MSG_TYPE_GROUP_MASK) == H9FRAME_BOOTLOADER_MSG_TYPE_GROUP) {
            cm_res.type = H9FRAME_TYPE_PAGE_FILL_BREAK;
            cm_res.dlc = 0;

            CAN_put_msg_blocking(&cm_res);
            break;
        }
    }
}
#else
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

void write_block(uint16_t block, uint8_t dst_id) {
    uint16_t bytes_remain = FLASH_BLOCK_SIZE;
    uint24_t tblptr = block * FLASH_BLOCK_SIZE;

    erase_block(tblptr);
    
    TBLPTR = tblptr;
	EECON1 = 0x84;       // Setup writes
    while (1) {
        h9frame_t cm;
        CAN_get_msg_blocking(&cm);

        h9frame_t cm_res;
        cm_res.unicast.seqnum = seqnum++;
        cm_res.unicast.destination_id = dst_id;
        cm_res.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;

        if (cm.source_id == dst_id && cm.type == H9FRAME_TYPE_PAGE_FILL && cm.dlc == 8) {
            for (uint8_t i = 0; i < cm.dlc; ++i) {
                TABLAT = cm.data[i];
                --bytes_remain;
                asm("TBLWT *+");
            }

            if (bytes_remain == 0) {
                cm_res.type = H9FRAME_TYPE_PAGE_WRITED;
                cm_res.dlc = 2;
                cm_res.data[0] = (block >> 8) & 0xff;
                cm_res.data[1] = (block) & 0xff;

                asm("TBLRD *-");
                StartWrite();
                //asm("TBLRD *+");
                EECON1bits.WREN = 0;
                CAN_put_msg_blocking(&cm_res);
                break;
            }
            else {
                cm_res.type = H9FRAME_TYPE_PAGE_FILL_NEXT;
                cm_res.dlc = 2;
                cm_res.data[0] = (bytes_remain >> 8) & 0xff;
                cm_res.data[1] = (bytes_remain) & 0xff;
                CAN_put_msg_blocking(&cm_res);
            }
        }
        else if (cm.source_id == dst_id && (cm.type & H9FRAME_BOOTLOADER_MSG_TYPE_GROUP_MASK) == H9FRAME_BOOTLOADER_MSG_TYPE_GROUP) {
            cm_res.type = H9FRAME_TYPE_PAGE_FILL_BREAK;
            cm_res.dlc = 0;

            CAN_put_msg_blocking(&cm_res);
            break;
        }
    }
}
#endif

void main(void) {
    H9PIC_INTCON.GIEL = 0;
    H9PIC_INTCON.GIEH = 0;
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
    turn_on_msg.data[6] = H9PIC_NODE_MCU;
#if _XTAL_FREQ == 16000000
    turn_on_msg.data[7] = NODE_MCU_F_16MHz;
#else
#error "Unknow node MCU_F"
#endif
    
    CAN_put_msg_blocking(&turn_on_msg);
    
    h9frame_t cm;
    while (1) {
        if (CAN_get_msg_blocking(&cm)) {
            if (cm.type == H9FRAME_TYPE_PAGE_START && cm.dlc == 2) {
                uint16_t block = (uint16_t)(cm.data[0] << 8) | cm.data[1];

                h9frame_t cm_res;

                cm_res.type = H9FRAME_TYPE_PAGE_FILL_NEXT;
                cm_res.unicast.seqnum = seqnum++;
                cm_res.unicast.destination_id = cm.source_id;
                cm_res.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;
                
                cm_res.dlc = 2;
                cm_res.data[0] = (FLASH_BLOCK_SIZE >> 8) & 0xff;
                cm_res.data[1] = (FLASH_BLOCK_SIZE) & 0xff;

                CAN_put_msg_blocking(&cm_res);

                write_block(block, cm.source_id);
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
