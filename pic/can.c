/*
 * h9pic-can v0.2
 *
 * Created by SQ8KFH on 2020-07-11.
 *
 * Copyright (C) 2020-2021 Kamil Palkowski. All rights reserved.
 */

#include <string.h>
#include "h9pic/can.h"
#include "h9pic/node_id.h"
#include "h9pic/bl_info.h"
#include "h9frame.h"
#include "h9def.h"

#include "h9pic/common.h"
#include "hw.h"


static struct {
    uint8_t node_id;
    uint16_t node_type;
    uint8_t pcb_revision;
    uint8_t bom_revision;
    uint16_t version_major;
    uint16_t version_minor;
    uint16_t version_patch;
    char build_info[H9FRAME_MAX_REGISTER_SIZE];
    volatile union {                        // NODE_FLAG_* in h9def.h, register 0, NODE_INFO data[6..7]
        struct {
            unsigned reset_reason : 3;      //0-2
            unsigned bl_present : 1;        //3
            unsigned bl_mismatch : 1;       //4
            unsigned default_id : 1;        //5
            unsigned can_error_warning : 1; //6, sticky
            unsigned can_tx_frame_loss : 1; //7, sticky
            unsigned can_rx_frame_loss : 1; //8, sticky
        };
        uint16_t raw;
    } flags;
} node_info;


void (*read_power_supply_register)(uint8_t, uint8_t) = NULL;

static void calc_can_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, h9frame_t *cm);
static void calc_can_unicast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t flags, uint8_t src, uint8_t dst, uint8_t seq);
static void calc_can_broadcast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t src, uint16_t node_type);
static void send_reg_value1(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value);
static void send_reg_value2(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2);
static void send_reg_value3(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3);
static void send_reg_value4(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3, uint8_t value4);
static void send_reg_value6(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3, uint8_t value4, uint8_t value5, uint8_t value6);
static void CAN_send_node_info_broadcast(uint8_t type);
static void process_standard_reg(h9frame_t *cm);
static uint8_t process_msg(h9frame_t *cm);


uint8_t CAN_init(uint16_t node_type, uint8_t default_id, uint8_t pcb_rev, uint8_t bom_rev, uint16_t version_major, uint16_t version_minor, uint16_t version_patch, const char *build_info) {
    uint8_t ret = 1;
    //TODO: mozna doddac STACK FULL kiedy braknie nam stosu i STACK UNDERFLOW kiedy to zepsulismy stos i return nie zadzialal
//    if (STKPTRbits.STKFUL) {
//        reset_reason = NODE_RESET_BY_STACK_OVERFLOW;
//        STKPTRbits.STKFUL = 0;  // wyczyść ręcznie
//    } else if (STKPTRbits.STKUNF) {
//        reset_reason = NODE_RESET_BY_STACK_UNDERFLOW;
//        STKPTRbits.STKUNF = 0;
//    }
    node_info.flags.reset_reason = mcu_reset_reason();
    
    node_info.node_type = node_type;
    node_info.pcb_revision = pcb_rev;
    node_info.bom_revision = bom_rev;
    node_info.version_major = version_major;
    node_info.version_minor = version_minor;
    node_info.version_patch = version_patch;
    strncpy(node_info.build_info, build_info, H9FRAME_MAX_REGISTER_SIZE);
    
    node_id_repair();          // restore a damaged EEPROM copy of the node id (writes only if needed)
    node_info.node_id = read_node_id();
    if (node_info.node_id == 0) {       // no valid id in EEPROM
        node_info.node_id = default_id;
        ret = 0;
    }

    node_info.flags.default_id = !ret;

    h9_bl_info_t bl;
    if (read_bl_info(&bl)) {
        node_info.flags.bl_present = 1;
        node_info.flags.bl_mismatch = bl.node_type != node_type || bl.pcb_rev != pcb_rev || bl.bom_rev != bom_rev;
    }

    can_hw_init(node_info.node_id, node_type);

    return ret;
}

uint8_t CAN_bus_error_warning(void) {
    return can_hw_error_warning();
}

void CAN_send_turned_on_broadcast(void) {
    CAN_send_node_info_broadcast(H9FRAME_TYPE_NODE_TURNED_ON);
}

void CAN_set_msg_filter_1(uint16_t broadcast_group) {
    can_hw_set_broadcast_filter(1, broadcast_group);
}

void CAN_set_msg_filter_2(uint16_t broadcast_group) {
    can_hw_set_broadcast_filter(2, broadcast_group);
}

uint8_t CAN_put_msg(h9frame_t *cm) {
    uint8_t id[4];
    
    //TODO: ujednolicic gdzie ma bys ustawiane source_id i broadcast.group
    cm->source_id = node_info.node_id;
    
    if (cm->type & H9FRAME_UNICAST_BROADCAST_BIT) {
        cm->broadcast.group = node_info.node_type;
        calc_can_broadcast_id(&id[0], &id[1], &id[2], &id[3], cm->type, cm->source_id, cm->broadcast.group);
    }
    else
        calc_can_unicast_id(&id[0], &id[1], &id[2], &id[3], cm->type, cm->source_id, cm->unicast.flags, cm->unicast.destination_id, cm->unicast.seqnum);

    for (;;) {
        uint8_t irq = mcu_irq_disable();

        if (can_hw_tx(id, cm->dlc, cm->data, cm->type == H9FRAME_TYPE_NODE_FAULT)) {
            mcu_irq_restore(irq);
            return 1;
        }
        else if (can_hw_bus_error()) {
            uint8_t fault_id[4];
            uint8_t fault_data[8] = {NODE_FAULT_CAN_FRAME_LOSS};
            calc_can_broadcast_id(&fault_id[0], &fault_id[1], &fault_id[2], &fault_id[3], H9FRAME_TYPE_NODE_FAULT, node_info.node_id, node_info.node_type);
            can_hw_tx_on_bus_error(fault_id, 1, fault_data);
            node_info.flags.can_tx_frame_loss = 1;      // interrupts disabled here

            mcu_irq_restore(irq);
            return 0;
        }

        // all TX buffers busy: re-enable interrupts so RX keeps being serviced while waiting
        mcu_irq_restore(irq);
    }
}

void send_command_error(uint8_t errno, uint8_t destination, uint8_t seqnum) {
    h9frame_t cm;
    cm.type = H9FRAME_TYPE_COMMAND_ERROR;
    cm.unicast.flags = 0;
    cm.unicast.destination_id = destination;
    cm.unicast.seqnum = seqnum;

    cm.data[0] = errno;
    cm.dlc = 1;
    CAN_put_msg(&cm);
}

void send_node_fault(uint8_t errno) {
    h9frame_t cm;
    cm.type = H9FRAME_TYPE_NODE_FAULT;

    cm.data[0] = errno;
    cm.dlc = 1;
    CAN_put_msg(&cm); 
}

//          | SIDH                        | SIDL                    | EIDH                    | EIDL
// 31 30 29 | 28     27 26 25 24 23 22 21 | 20 19 18 ** ** ** 17 16 | 15 14 13 12 11 10 09 08 | 07 06 05 04 03 02 01 00
// -- -- -- | ty_(0) ty ty ty ty so so so | so so so **  1 ** so so | fl fl fl ds ds ds ds ds | ds ds ds sq sq sq sq sq
// -- -- -- | ty_(1) ty ty ty ty so so so | so so so **  1 ** so so | nt nt nt nt nt nt nt nt | nt nt nt nt nt nt nt nt
uint8_t CAN_get_msg(h9frame_t* cm) {
    if (CAN_bus_error_warning()) {
        uint8_t irq = mcu_irq_disable();     // flags are also set from interrupts, keep the read-modify-write atomic
        node_info.flags.can_error_warning = 1;
        mcu_irq_restore(irq);
    }

    if (can_hw_rx_overflow()) {
        uint8_t irq = mcu_irq_disable();
        node_info.flags.can_rx_frame_loss = 1;
        mcu_irq_restore(irq);
        send_node_fault(NODE_FAULT_CAN_RX_FRAME_LOSS);
    }

    can_buf_t frame;
    if (can_hw_rx(&frame)) {
        can_buf_t *b = &frame;
        cm->type = b->id[0] >> 3;
        cm->source_id = (uint8_t)(b->id[0] << 5) | (uint8_t)(b->id[1] >> 3 & 0x1c) | (uint8_t)(b->id[1] & 0x03);
        
        if (cm->type & H9FRAME_UNICAST_BROADCAST_BIT)
            cm->broadcast.group = (uint16_t)(b->id[2] << 8) | (uint16_t)(b->id[3]);
        else {
            cm->unicast.flags = b->id[2] >> 5;
            cm->unicast.destination_id = (uint8_t)(b->id[2] << 3) | (uint8_t)(b->id[3] >> 5);
            cm->unicast.seqnum = b->id[3] & 0x1f;
        }

        cm->dlc = b->dlc & 0x0f;
        uint8_t idx = 0;
        for (; idx < 8; ++idx)
            cm->data[idx] = b->data[idx];

        return process_msg(cm);
    }
     
    return 0;
}

void CAN_send_reg_value(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t *value, size_t length) {
    h9frame_t cm;
    cm.type = H9FRAME_TYPE_REG_VALUE;
    cm.unicast.destination_id = destination;
    cm.unicast.seqnum = seqnum;

    size_t value_ix = 0;

    for (uint8_t msg_num = 0; value_ix < length; ++msg_num) {
        uint8_t i = 1;
        cm.data[0] = registry;
        
        if (length < 8) {
            cm.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;
        }
        else if (msg_num == 0) {
            cm.unicast.flags = H9FRAME_FLAG_MULTI_FRAME_FIRST;
            cm.data[1] = (uint8_t)((length + 5) / 6);
            i++;
        }
        else if (value_ix < length - 6) {
            cm.unicast.flags = H9FRAME_FLAG_MULTI_FRAME_MIDDLE;
            cm.data[1] = msg_num;
            i++;
        }
        else {
            cm.unicast.flags = H9FRAME_FLAG_MULTI_FRAME_LAST;
            cm.data[1] = msg_num;
            i++;
        }
        
        for (; i < 8 && value_ix < length; ++i) {
            cm.data[i] = value[value_ix];
            value_ix++;
        }
        cm.dlc = i;

        CAN_put_msg(&cm);
    }
}

static void send_reg_value1(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value) {
    h9frame_t cm;
    cm.type = H9FRAME_TYPE_REG_VALUE;
    cm.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;
    cm.unicast.destination_id = destination;
    cm.unicast.seqnum = seqnum;

    cm.data[0] = registry;
    cm.data[1] = value;
    cm.dlc = 2;
    CAN_put_msg(&cm);
}

static void send_reg_value2(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2) {
    h9frame_t cm;

    cm.type = H9FRAME_TYPE_REG_VALUE;
    cm.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;
    cm.unicast.destination_id = destination;
    cm.unicast.seqnum = seqnum;

    cm.data[0] = registry;
    cm.data[1] = value1;
    cm.data[2] = value2;
    cm.dlc = 3;
    CAN_put_msg(&cm);
}

static void send_reg_value3(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3) {
    h9frame_t cm;
    cm.type = H9FRAME_TYPE_REG_VALUE;
    cm.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;
    cm.unicast.destination_id = destination;
    cm.unicast.seqnum = seqnum;

    cm.data[0] = registry;
    cm.data[1] = value1;
    cm.data[2] = value2;
    cm.data[3] = value3;
    cm.dlc = 4;
    CAN_put_msg(&cm);
}

static void send_reg_value4(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3, uint8_t value4) {
    h9frame_t cm;
    cm.type = H9FRAME_TYPE_REG_VALUE;
    cm.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;
    cm.unicast.destination_id = destination;
    cm.unicast.seqnum = seqnum;

    cm.data[0] = registry;
    cm.data[1] = value1;
    cm.data[2] = value2;
    cm.data[3] = value3;
    cm.data[4] = value4;
    cm.dlc = 5;
    CAN_put_msg(&cm);
}

static void send_reg_value6(uint8_t registry, uint8_t destination, uint8_t seqnum, uint8_t value1, uint8_t value2, uint8_t value3, uint8_t value4, uint8_t value5, uint8_t value6) {
    h9frame_t cm;
    cm.type = H9FRAME_TYPE_REG_VALUE;
    cm.unicast.flags = H9FRAME_FLAG_SINGE_FRAME;
    cm.unicast.destination_id = destination;
    cm.unicast.seqnum = seqnum;

    cm.data[0] = registry;
    cm.data[1] = value1;
    cm.data[2] = value2;
    cm.data[3] = value3;
    cm.data[4] = value4;
    cm.data[5] = value5;
    cm.data[6] = value6;
    cm.dlc = 7;
    CAN_put_msg(&cm);
}

// type: H9FRAME_TYPE_NODE_INFO or H9FRAME_TYPE_NODE_TURNED_ON
static void CAN_send_node_info_broadcast(uint8_t type) {
    h9frame_t cm;

    cm.type = type;

    // version packed into 32 bits: major (10 bits) | minor (11 bits) | patch (11 bits), same as BOOTLOADER_TURNED_ON
    uint32_t version = ((uint32_t)(node_info.version_major & 0x3ff) << 22) | ((uint32_t)(node_info.version_minor & 0x7ff) << 11) | (node_info.version_patch & 0x7ff);
    uint16_t flags = node_info.flags.raw;

    cm.dlc = 8;
    cm.data[0] = (uint8_t)(version >> 24);
    cm.data[1] = (uint8_t)(version >> 16);
    cm.data[2] = (uint8_t)(version >> 8);
    cm.data[3] = (uint8_t)version;
    cm.data[4] = node_info.pcb_revision;
    cm.data[5] = node_info.bom_revision;
    cm.data[6] = (uint8_t)(flags >> 8);
    cm.data[7] = (uint8_t)flags;
    CAN_put_msg(&cm);
}

static uint32_t read_serial_numer(void) {
    //doc: doc/SN.md
    uint32_t sn = 0;
    
    // index 0-7 → adresy 0x200000-0x200007
    TBLPTRU = 0x20;          // górny bajt adresu
    TBLPTRH = 0x00;
    TBLPTRL = 1;            // 0x00-0x07

    for (int i = 0; i < 4; ++i) {
        sn <<= 8;
        asm("TBLRD*+"); // odczyt do TABLAT i inkremantacja
        sn |= TABLAT;
    }
    return sn;
}

static void process_standard_reg(h9frame_t *cm) {
    if (cm->type == H9FRAME_TYPE_SET_REG && cm->dlc > 1) {
        switch (cm->data[0]) {
            case NODE_FLAGS_STD_REGISTER:
            case NODE_TYPE_STD_REGISTER:
            case NODE_HARDWARE_REVISION_STD_REGISTER:
            case NODE_VERSION_STD_REGISTER:
            case NODE_BUILD_INFO_STD_REGISTER:
            case NODE_MCU_TYPE_STD_REGISTER:
            case NODE_SN_STD_REGISTER:
            case NODE_POWER_SUPPLY_STD_REGISTER:
            //case NODE_MCU_TEMP_STD_REGISTER,
                send_command_error(H9FRAME_ERROR_READ_ONLY_REGISTER, cm->source_id, cm->unicast.seqnum);
                return;
            case NODE_ID_STD_REGISTER:
                if (cm->dlc == 2) {
                    write_node_id(cm->data[1]);
                    send_reg_value1(NODE_ID_STD_REGISTER, cm->source_id, cm->unicast.seqnum, node_info.node_id);
                    return;
                }
                else {
                    send_command_error(H9FRAME_ERROR_REGISTER_SIZE_MISMATCH, cm->source_id, cm->unicast.seqnum);
                    return;
                }
            default:
                send_command_error(H9FRAME_ERROR_UNSUPPORTED_REGISTER, cm->source_id, cm->unicast.seqnum);
                return;
        }
    }
    else if (cm->type == H9FRAME_TYPE_GET_REG && cm->dlc == 1) {
        switch (cm->data[0]) {
            case NODE_FLAGS_STD_REGISTER: {
                uint16_t flags = node_info.flags.raw;
                send_reg_value2(NODE_FLAGS_STD_REGISTER, cm->source_id, cm->unicast.seqnum, (uint8_t)(flags >> 8), (uint8_t)flags);
                return;
            }
            case NODE_TYPE_STD_REGISTER:
                send_reg_value2(NODE_TYPE_STD_REGISTER, cm->source_id, cm->unicast.seqnum, (node_info.node_type >> 8) & 0xff, (node_info.node_type) & 0xff);
                return;
            case NODE_HARDWARE_REVISION_STD_REGISTER:
                send_reg_value2(NODE_HARDWARE_REVISION_STD_REGISTER, cm->source_id, cm->unicast.seqnum, node_info.pcb_revision, node_info.bom_revision);
                return;
            case NODE_VERSION_STD_REGISTER:
                send_reg_value6(NODE_VERSION_STD_REGISTER, cm->source_id, cm->unicast.seqnum, (node_info.version_major >> 8), node_info.version_major & 0xff, (node_info.version_minor >> 8) & 0xff, node_info.version_minor & 0xff, (node_info.version_patch >> 8) & 0xff, node_info.version_patch & 0xff);
                return;
            case NODE_BUILD_INFO_STD_REGISTER: {
                size_t len = 0;
                for (; node_info.build_info[len] && len < (H9FRAME_MAX_REGISTER_SIZE - 1); len++);
                CAN_send_reg_value(NODE_BUILD_INFO_STD_REGISTER, cm->source_id, cm->unicast.seqnum, (uint8_t*)&node_info.build_info, len);
                return;
            }
            case NODE_MCU_TYPE_STD_REGISTER:
#if defined (_18F46K80)
                send_reg_value1(NODE_MCU_TYPE_STD_REGISTER, cm->source_id, cm->unicast.seqnum, NODE_MCU_PIC18F46K80);
#elif defined (_18F47Q84)
                send_reg_value1(NODE_MCU_TYPE_STD_REGISTER, cm->source_id, cm->unicast.seqnum, NODE_MCU_PIC18F47Q84);
#else
#error Unsupported MCU
#endif
                return;
            case NODE_SN_STD_REGISTER: {
                uint32_t sn = read_serial_numer();
                send_reg_value4(NODE_SN_STD_REGISTER, cm->source_id, cm->unicast.seqnum, (uint8_t)(sn >> 24), (uint8_t)(sn >> 16), (uint8_t)(sn >> 8), (uint8_t)(sn));
                return;
            }
            case NODE_POWER_SUPPLY_STD_REGISTER:
                if (read_power_supply_register) {
                    read_power_supply_register(cm->source_id, cm->unicast.seqnum);
                }
                else {
                    send_command_error(H9FRAME_ERROR_UNSUPPORTED_REGISTER, cm->source_id, cm->unicast.seqnum);
                }
                return;
            //case NODE_MCU_TEMP_STD_REGISTER:
            //     return;
            case NODE_ID_STD_REGISTER:
                send_reg_value1(NODE_ID_STD_REGISTER, cm->source_id, cm->unicast.seqnum, node_info.node_id);
                return;
            default:
                send_command_error(H9FRAME_ERROR_UNSUPPORTED_REGISTER, cm->source_id, cm->unicast.seqnum);
                return;
        }
    }
    //H9FRAME_TYPE_SET_BIT, H9FRAME_TYPE_CLEAR_BIT
    send_command_error(H9FRAME_ERROR_UNSUPPORTED_OPERATION, cm->source_id, cm->unicast.seqnum);
}

static uint8_t process_msg(h9frame_t *cm) {
    /* --- BROADCAST --- */
    if (cm->type & H9FRAME_UNICAST_BROADCAST_BIT) {
        if (cm->type == H9FRAME_TYPE_DISCOVER || cm->type == H9FRAME_TYPE_GROUP_RESET) {
            if (cm->broadcast.group == node_info.node_type || cm->broadcast.group == H9FRAME_BROADCAST_ALL_GROUP) {
                if (cm->type == H9FRAME_TYPE_DISCOVER) {
                    CAN_send_node_info_broadcast(H9FRAME_TYPE_NODE_INFO);
                    return 0;
                }
                else if (cm->type == H9FRAME_TYPE_GROUP_RESET) {
                    RESET();
                    return 0;
                }
            }
            else {
                // INVALID_MSG but we don't answere on broadcast
                return 0;
            }
        }

        return 2;
    }
    /* --- UNICAST --- */
    else {
        if (cm->unicast.destination_id != node_info.node_id) {
            return 0; //not for me
        }

        /* -- RCV BOOTLOADER MSG -- */
        if (cm->type <= H9FRAME_TYPE_PAGE_FILL_BREAK) {
            send_command_error(H9FRAME_ERROR_INVALID_FRAME, cm->source_id, cm->unicast.seqnum);
            return 0;
        }

        /* -- MULTIPLE MSG -- */
        if (cm->unicast.flags != 0 && cm->type != H9FRAME_TYPE_SET_REG && cm->type != H9FRAME_TYPE_REG_VALUE) {
            send_command_error(H9FRAME_ERROR_INVALID_FRAME, cm->source_id, cm->unicast.seqnum);
            return 0;
        }

        if (cm->type == H9FRAME_TYPE_NODE_RESET) {
            RESET();
            return 0;
        }
        else if (cm->type == H9FRAME_TYPE_NODE_UPGRADE && cm->dlc == 0) {
            di();
            STKPTR = 0x00;
#if defined (_18F46K80)
            asm ("goto 0xF600");    // pic_bootloader code offset
#elif defined (_18F47Q84)
            asm ("goto 0x1E000");   // pic_bootloader code offset, last 8 KB of flash
#else
#error Unsupported MCU
#endif
        }
        else if (cm->type == H9FRAME_TYPE_SET_REG || cm->type == H9FRAME_TYPE_GET_REG || cm->type == H9FRAME_TYPE_SET_BIT || cm->type == H9FRAME_TYPE_CLEAR_BIT) {
            /* --- STANDARD REG OPERATION -- */
            if (cm->dlc > 0 && cm->data[0] < 10) {
                process_standard_reg(cm);
                return 0;
            }
            else {
                return 1;
            }
        }
        return 1; //THEORETICALLY H9FRAME_TYPE_COMMAND_ERROR OR H9FRAME_TYPE_REG_VALUE
    }
}
