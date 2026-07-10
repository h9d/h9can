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

#define FLASH_BLOCK_SIZE 64
#define BOOTLOADER_VERSION_MAJOR 1
#define BOOTLOADER_VERSION_MINOR 0


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

            CAN_put_msg_blocking(&cm_res);
            break;
        }
    }
}

void main(void) {
    INTCONbits.PEIE = 0;
    INTCONbits.GIE = 0;
    CAN_init();
    
    h9frame_t turn_on_msg;

    turn_on_msg.type = H9FRAME_TYPE_BOOTLOADER_TURNED_ON;
    turn_on_msg.dlc = 8;
    turn_on_msg.data[0] = 0;
    turn_on_msg.data[1] = 0;
    turn_on_msg.data[2] = (BOOTLOADER_VERSION_MAJOR >> 8);
    turn_on_msg.data[3] = BOOTLOADER_VERSION_MAJOR & 0xff;
    turn_on_msg.data[4] = (BOOTLOADER_VERSION_MINOR >> 8) & 0xff;
    turn_on_msg.data[5] = BOOTLOADER_VERSION_MINOR & 0xff;
    turn_on_msg.data[6] = NODE_MCU_PIC18F46K80;
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

                cm_res.type = H9FRAME_TYPE_PAGE_FILL;
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
            turn_on_msg.unicast.seqnum = seqnum++;
            CAN_put_msg_blocking(&turn_on_msg);
        }
    }
    return;
}
