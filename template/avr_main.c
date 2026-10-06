/*
 *
 *
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <h9can.h>
#include <h9def.h>

#include "version.h"

static void can_proc(void);

int main(void) {
	DDRB = 0xff;
	DDRC = 0xff;
	DDRD = 0xff;
	DDRE = 0xff;

	// NODE_TYPE, PCB_REVISION, BOM_REVISION are defined in CMakeLists.txt
	CAN_init(NODE_TYPE, 0xff - NODE_TYPE, PCB_REVISION, BOM_REVISION, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, APP_VERSION);

	sei();

	CAN_send_turned_on_broadcast();

    while (1) {
        can_proc();
	}
}

static void can_proc(void) {
    h9frame_t cm;
    int can_get_ret = CAN_get_msg(&cm);
    if (can_get_ret == 1) {
        if (cm.type == H9FRAME_TYPE_GET_REG) {
            uint16_t tmp;
            switch (cm.data[0]) {
                // case 10: //STATUS
                //     tmp = (registers.status.raw >> 8) | (registers.status.raw << 8);
                //     CAN_send_reg_value(cm.data[0], cm.source_id, cm.unicast.seqnum, (uint8_t *)&tmp, 2);
                //     break;
                // case 11: //CTRL
                //     CAN_send_reg_value(cm.data[0], cm.source_id, cm.unicast.seqnum, (uint8_t *)&registers.ctrl.raw, 1);
                //     break;
                default:
                    send_command_error(H9FRAME_ERROR_INVALID_REGISTER, cm.source_id, cm.unicast.seqnum);
                    break;
            }
        } else if (cm.type == H9FRAME_TYPE_SET_REG) {
            switch (cm.data[0]) {
                case 10: //STATUS
                    send_command_error(H9FRAME_ERROR_READ_ONLY_REGISTER, cm.source_id, cm.unicast.seqnum);
                    break;
                // case 11: //CTRL
                //     //TOTO: register size check
                //     atu_set_ctrl(cm.data[1]);
                //     CAN_send_reg_value(cm.data[0], cm.source_id, cm.unicast.seqnum, (uint8_t *)&registers.ctrl.raw, 1);
                //     break;
                default:
                    send_command_error(H9FRAME_ERROR_INVALID_REGISTER, cm.source_id, cm.unicast.seqnum);
                    break;
            }
        }
        else if (cm.type == H9FRAME_TYPE_SET_BIT) {
            switch (cm.data[0]) {
                case 10: //STATUS
                    send_command_error(H9FRAME_ERROR_READ_ONLY_REGISTER, cm.source_id, cm.unicast.seqnum);
                    break;
                // case 11: //CTRL
                //     //TOTO: register size check
                //     atu_set_ctrl(registers.ctrl.raw | (uint8_t)(1 << cm.data[1]));
                //     CAN_send_reg_value(cm.data[0], cm.source_id, cm.unicast.seqnum, (uint8_t *)&registers.ctrl.raw, 1);
                //     break;
                default:
                    send_command_error(H9FRAME_ERROR_INVALID_REGISTER, cm.source_id, cm.unicast.seqnum);
                    break;
            }
        }
        else if (cm.type == H9FRAME_TYPE_CLEAR_BIT) {
            switch (cm.data[0]) {
                case 10: //STATUS
                    send_command_error(H9FRAME_ERROR_READ_ONLY_REGISTER, cm.source_id, cm.unicast.seqnum);
                    break;
                // case 11: //CTRL
                //     atu_set_ctrl(registers.ctrl.raw & ~(uint8_t)(1 << cm.data[1]));
                //     CAN_send_reg_value(cm.data[0], cm.source_id, cm.unicast.seqnum, (uint8_t *)&registers.ctrl, 1);
                //     break;
                default:
                    send_command_error(H9FRAME_ERROR_INVALID_REGISTER, cm.source_id, cm.unicast.seqnum);
                    break;
            }
        } else {
            send_command_error(H9FRAME_ERROR_INVALID_FRAME, cm.source_id, cm.unicast.seqnum);
        }
    } else if (can_get_ret == 2) {
        //            if (cm.source_id == atu_node_id) {
        //                process_atu_msg(&cm);
        //                screen_refresh();
        //            }
    }
}
