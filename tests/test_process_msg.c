#include <string.h>
#include <stdint.h>

#include "unity.h"
#include "h9frame.h"
#include "h9def.h"

/* Symbols from can.c */
extern volatile uint8_t can_node_id;
extern uint8_t process_msg(h9frame_t *cm);
extern void CAN_init(uint16_t node_type, char hardware_rev,
                     uint16_t version_major, uint16_t version_minor,
                     const char *build_info);
extern void set_mandatory_message_fields(h9frame_t *cm);
/* ------------------------------------------------------------------ */
/* CAN_put_msg mock — strong symbol overrides the weak one in can.c   */
/* ------------------------------------------------------------------ */
#define MAX_SENT 33
static h9frame_t  g_sent[MAX_SENT];
static int      g_send_count;
static int      reset_flag;

uint8_t CAN_put_msg(h9frame_t *cm) {
    set_mandatory_message_fields(cm);
    if (g_send_count < MAX_SENT)
        g_sent[g_send_count++] = *cm;
    return 1;
}

void mcu_reset(void) {
    reset_flag = 1;
}

static void reset_capture(void) {
    g_send_count = 0;
    memset(g_sent, 0, sizeof(g_sent));
    reset_flag = 0;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
#define TEST_SRC_ID      1

#define MY_NODE_ID       42
#define MY_NODE_TYPE     5
#define MY_HARDWARE_REV  'x'
#define MY_VERSION_MAJOR 1013
#define MY_VERSION_MINOR 1313
#define MY_BUILD_INFO    "v113.114-115-gde2b8be-dirt"


static h9frame_t make_get_reg(uint8_t reg, uint8_t seqnum) {
    h9frame_t m;
    memset(&m, 0, sizeof(m));
    m.type                   = H9FRAME_TYPE_GET_REG;
    m.unicast.destination_id = MY_NODE_ID;
    m.source_id              = TEST_SRC_ID;
    m.unicast.seqnum         = seqnum;
    m.dlc                    = 1;
    m.data[0]                = reg;
    return m;
}

static h9frame_t make_set_reg(uint8_t reg, const uint8_t *val, uint8_t vlen, uint8_t seqnum) {
    h9frame_t m;
    memset(&m, 0, sizeof(m));
    m.type                   = H9FRAME_TYPE_SET_REG;
    m.unicast.destination_id = MY_NODE_ID;
    m.source_id              = TEST_SRC_ID;
    m.unicast.seqnum         = seqnum;
    m.dlc                    = (uint8_t)(1 + vlen);
    m.data[0]                = reg;
    for (uint8_t i = 0; i < vlen; i++) m.data[1 + i] = val[i];
    return m;
}

/* ------------------------------------------------------------------ */
/* Unity lifecycle                                                     */
/* ------------------------------------------------------------------ */
void setUp(void) {
    reset_capture();
    can_node_id = MY_NODE_ID;
}

void tearDown(void) {
}

/* ------------------------------------------------------------------ */
/* Tests — process_msg return value                                    */
/* ------------------------------------------------------------------ */

void test_non_standard_reg_returned_to_app(void) {
    h9frame_t in = make_get_reg(10, 0);   /* reg >= 10 → returned to application */
    TEST_ASSERT_EQUAL_UINT8(1, process_msg(&in));
    TEST_ASSERT_EQUAL_INT(0, g_send_count);
}

/* ------------------------------------------------------------------ */
/* Tests — process_standard_reg: compare captured h9frame_t with pattern*/
/* ------------------------------------------------------------------ */

void test_bootloader_type_msg(void) {
    h9frame_t in;
    memset(&in, 0, sizeof(in));
    in.type                   = H9FRAME_TYPE_PAGE_START;
    in.unicast.destination_id = MY_NODE_ID;
    in.source_id              = TEST_SRC_ID;
    in.unicast.seqnum         = 21;
    in.dlc                    = 2;

    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));
    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,            g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,                g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                          g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                         g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(21,                                  g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(H9FRAME_ERROR_INVALID_FRAME,           g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT(1,                                   g_sent[0].dlc);
}

void test_unicast_dst_id_mismatch(void) {
    h9frame_t in = make_get_reg(NODE_TYPE_STD_REGISTER, 7);
    in.unicast.destination_id = MY_NODE_ID + 1;

    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));
    TEST_ASSERT_EQUAL_INT(0, g_send_count);
}

void test_broadcast_group_mismatch_discover(void) {
    h9frame_t in;
    memset(&in, 0, sizeof(in));
    in.type                   = H9FRAME_TYPE_DISCOVER;
    in.broadcast.group        = MY_NODE_TYPE + 1;
    in.dlc                    = 0;

    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));
    TEST_ASSERT_EQUAL_INT(0, g_send_count);
}

void test_broadcast_group_mismatch_reset(void) {
    h9frame_t in;
    memset(&in, 0, sizeof(in));
    in.type                   = H9FRAME_TYPE_GROUP_RESET;
    in.broadcast.group        = MY_NODE_TYPE + 1;
    in.dlc                    = 0;

    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));
    TEST_ASSERT_EQUAL_INT(0, g_send_count);

    TEST_ASSERT_EQUAL_INT(0, reset_flag);
}

void test_broadcast_group_mismatch_reg_value(void) {
    h9frame_t in;
    memset(&in, 0, sizeof(in));
    in.type                   = H9FRAME_TYPE_REG_VALUE_BROADCAST;
    in.broadcast.group        = MY_NODE_TYPE + 1;
    in.dlc                    = 2;

    TEST_ASSERT_EQUAL_UINT8(1, process_msg(&in));
    TEST_ASSERT_EQUAL_INT(0, g_send_count);
}

void test_broadcast_group_discover(void) {
    h9frame_t in;
    memset(&in, 0, sizeof(in));
    in.type                   = H9FRAME_TYPE_DISCOVER;
    in.broadcast.group        = MY_NODE_TYPE;
    in.dlc                    = 0;

    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));
    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_NODE_INFO,                   g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                             g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(MY_NODE_TYPE,                           g_sent[0].broadcast.group);

    TEST_ASSERT_EQUAL_INT((MY_NODE_TYPE >> 8) & 0xff,             g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT(MY_NODE_TYPE & 0xff,                    g_sent[0].data[1]);

    TEST_ASSERT_EQUAL_INT((MY_VERSION_MAJOR >> 8) & 0xff,         g_sent[0].data[2]);
    TEST_ASSERT_EQUAL_INT(MY_VERSION_MAJOR & 0xff,                g_sent[0].data[3]);
    TEST_ASSERT_EQUAL_INT((MY_VERSION_MINOR >> 8) & 0xff,         g_sent[0].data[4]);
    TEST_ASSERT_EQUAL_INT(MY_VERSION_MINOR & 0xff,                g_sent[0].data[5]);

    TEST_ASSERT_EQUAL_INT(MY_HARDWARE_REV,                        g_sent[0].data[6]);
    TEST_ASSERT_GREATER_OR_EQUAL_INT8(NODE_RESET_BY_UNKNOWN,       g_sent[0].data[7]);
    TEST_ASSERT_LESS_OR_EQUAL_INT8(NODE_RESET_BY_EXTERNAL_SOURCE, g_sent[0].data[7]);

    TEST_ASSERT_EQUAL_INT(8,                                      g_sent[0].dlc);
}

void test_broadcast_group_all_discover(void) {
    h9frame_t in;
    memset(&in, 0, sizeof(in));
    in.type                   = H9FRAME_TYPE_DISCOVER;
    in.broadcast.group        = H9FRAME_BROADCAST_ID;
    in.dlc                    = 0;

    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));
    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_NODE_INFO,                   g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                             g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(MY_NODE_TYPE,                           g_sent[0].broadcast.group);

    TEST_ASSERT_EQUAL_INT((MY_NODE_TYPE >> 8) & 0xff,             g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT(MY_NODE_TYPE & 0xff,                    g_sent[0].data[1]);

    TEST_ASSERT_EQUAL_INT((MY_VERSION_MAJOR >> 8) & 0xff,         g_sent[0].data[2]);
    TEST_ASSERT_EQUAL_INT(MY_VERSION_MAJOR & 0xff,                g_sent[0].data[3]);
    TEST_ASSERT_EQUAL_INT((MY_VERSION_MINOR >> 8) & 0xff,         g_sent[0].data[4]);
    TEST_ASSERT_EQUAL_INT(MY_VERSION_MINOR & 0xff,                g_sent[0].data[5]);

    TEST_ASSERT_EQUAL_INT(MY_HARDWARE_REV,                        g_sent[0].data[6]);
    TEST_ASSERT_GREATER_OR_EQUAL_INT8(NODE_RESET_BY_UNKNOWN,       g_sent[0].data[7]);
    TEST_ASSERT_LESS_OR_EQUAL_INT8(NODE_RESET_BY_EXTERNAL_SOURCE, g_sent[0].data[7]);

    TEST_ASSERT_EQUAL_INT(8,                                      g_sent[0].dlc);
}

void test_broadcast_group_reset(void) {
    h9frame_t in;
    memset(&in, 0, sizeof(in));
    in.type                   = H9FRAME_TYPE_GROUP_RESET;
    in.broadcast.group        = MY_NODE_TYPE;
    in.dlc                    = 0;

    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));
    TEST_ASSERT_EQUAL_INT(0, g_send_count);

    TEST_ASSERT_EQUAL_INT(1, reset_flag);
}
void test_broadcast_group_all_reset(void) {
    h9frame_t in;
    memset(&in, 0, sizeof(in));
    in.type                   = H9FRAME_TYPE_GROUP_RESET;
    in.broadcast.group        = H9FRAME_BROADCAST_ID;
    in.dlc                    = 0;

    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));
    TEST_ASSERT_EQUAL_INT(0, g_send_count);

    TEST_ASSERT_EQUAL_INT(1, reset_flag);
}

void test_set_bit_on_readonly(void) {
    h9frame_t in;
    memset(&in, 0, sizeof(in));
    in.type                   = H9FRAME_TYPE_SET_BIT;
    in.unicast.flags                  = H9FRAME_FLAG_SINGE_MSG;
    in.source_id              = TEST_SRC_ID;
    in.unicast.destination_id = MY_NODE_ID;
    in.unicast.seqnum         = 21;

    in.data[0]                = NODE_VERSION_STD_REGISTER;
    in.data[1]                = 1;
    in.dlc                    = 2;

    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));
    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,            g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,                g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                          g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                         g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(21,                                  g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(H9FRAME_ERROR_UNSUPPORTED_OPERATION, g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT(1,                                   g_sent[0].dlc);
}

void test_set_bit_on_writable(void) {
    h9frame_t in;
    memset(&in, 0, sizeof(in));
    in.type                   = H9FRAME_TYPE_SET_BIT;
    in.unicast.flags                  = H9FRAME_FLAG_SINGE_MSG;
    in.source_id              = TEST_SRC_ID;
    in.unicast.destination_id = MY_NODE_ID;
    in.unicast.seqnum         = 21;

    in.data[0]                = NODE_ID_STD_REGISTER;
    in.data[1]                = 1;
    in.dlc                    = 2;

    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));
    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,            g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,                g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                          g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                         g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(21,                                  g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(H9FRAME_ERROR_UNSUPPORTED_OPERATION, g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT(1,                                   g_sent[0].dlc);
}

/* ------------------------------------------------------------------ */
/* Tests — dev standard reg                                           */
/* ------------------------------------------------------------------ */

//NODE_TYPE_STD_REGISTER
void test_std_reg_node_type_read(void) {
    h9frame_t in = make_get_reg(NODE_TYPE_STD_REGISTER, 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_REG_VALUE,              g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,              g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                        g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                       g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                 g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_STD_REGISTER,            g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT((MY_NODE_TYPE >> 8) & 0xff,        g_sent[0].data[1]);
    TEST_ASSERT_EQUAL_INT(MY_NODE_TYPE & 0xff,               g_sent[0].data[2]);
    TEST_ASSERT_EQUAL_INT(3,                                 g_sent[0].dlc);
}

void test_std_reg_node_type_write(void) {
    uint16_t tmp = 1;
    uint8_t seq = (1 << H9FRAME_SEQNUM_BIT_LENGTH) - 1;
    h9frame_t in = make_set_reg(NODE_TYPE_STD_REGISTER, (uint8_t*)&tmp, 2, seq);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,          g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,              g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                        g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                       g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(seq,                               g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(H9FRAME_ERROR_READ_ONLY_REGISTER,  g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT(1,                                 g_sent[0].dlc);
}

//NODE_HARDWARE_REVISION_STD_REGISTER
void test_std_reg_node_hardware_revision_read(void) {
    h9frame_t in = make_get_reg(NODE_HARDWARE_REVISION_STD_REGISTER, 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_REG_VALUE,                g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,                g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                          g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                         g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                   g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(NODE_HARDWARE_REVISION_STD_REGISTER, g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT(MY_HARDWARE_REV,                     g_sent[0].data[1]);
    TEST_ASSERT_EQUAL_INT(2,                                   g_sent[0].dlc);
}

void test_std_reg_node_hardware_revision_write(void) {
    uint8_t tmp = 'c';
    h9frame_t in = make_set_reg(NODE_HARDWARE_REVISION_STD_REGISTER, &tmp, 1, 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,          g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,              g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                        g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                       g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                 g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(H9FRAME_ERROR_READ_ONLY_REGISTER,  g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT(1,                                 g_sent[0].dlc);
}

//NODE_VERSION_STD_REGISTER
void test_std_reg_node_versio_read(void) {
    h9frame_t in = make_get_reg(NODE_VERSION_STD_REGISTER, 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_REG_VALUE,              g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,              g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                        g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                       g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                 g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(NODE_VERSION_STD_REGISTER,         g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT((MY_VERSION_MAJOR >> 8) & 0xff,    g_sent[0].data[1]);
    TEST_ASSERT_EQUAL_INT(MY_VERSION_MAJOR & 0xff,           g_sent[0].data[2]);
    TEST_ASSERT_EQUAL_INT((MY_VERSION_MINOR >> 8) & 0xff,    g_sent[0].data[3]);
    TEST_ASSERT_EQUAL_INT(MY_VERSION_MINOR & 0xff,           g_sent[0].data[4]);
    TEST_ASSERT_EQUAL_INT(5,                                 g_sent[0].dlc);
}

void test_std_reg_node_versio_write(void) {
    uint16_t tmp[2] = {1, 2};
    h9frame_t in = make_set_reg(NODE_VERSION_STD_REGISTER, (uint8_t*)&tmp, sizeof(tmp), 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,          g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,              g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                        g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                       g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                 g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(H9FRAME_ERROR_READ_ONLY_REGISTER,  g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT(1,                                 g_sent[0].dlc);
}

//NODE_BUILD_INFO_STD_REGISTER
void test_std_reg_build_info_read(void) {
    uint8_t seq = 7;
    h9frame_t in = make_get_reg(NODE_BUILD_INFO_STD_REGISTER, seq);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    char build_info[] = MY_BUILD_INFO;
    char* build_info_ptr = build_info;
    uint8_t msg_count = (sizeof(build_info) + 6) / 7; //rounding up
    int bytes_left = sizeof(build_info);

    TEST_ASSERT_EQUAL_INT(msg_count, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_REG_VALUE,                  g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_MULTI_MSG_FIRST,            g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                            g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                           g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(seq,                                   g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(NODE_BUILD_INFO_STD_REGISTER,          g_sent[0].data[0]);

    TEST_ASSERT_EQUAL_INT((bytes_left > 7 ? 7 : bytes_left) + 1, g_sent[0].dlc);

    for (int i = 1; i < g_sent[0].dlc; i++) {
        TEST_ASSERT_EQUAL_INT(*build_info_ptr,                   g_sent[0].data[i]);
        bytes_left--;
        if (*build_info_ptr != '\0') {
            build_info_ptr++;
        }
    }

    int msg_num = 1;
    for (; msg_num < msg_count - 1; msg_num++) {
        TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_REG_VALUE,                   g_sent[msg_num].type);
        TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_MULTI_MSG_MIDDLE,            g_sent[msg_num].unicast.flags);
        TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                             g_sent[msg_num].source_id);
        TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                            g_sent[msg_num].unicast.destination_id);
        TEST_ASSERT_EQUAL_INT(seq,                                    g_sent[msg_num].unicast.seqnum);
        TEST_ASSERT_EQUAL_INT(msg_num,                                g_sent[msg_num].data[0]);

        TEST_ASSERT_EQUAL_INT(msg_num,                                g_sent[msg_num].data[0]);
        TEST_ASSERT_EQUAL_INT((bytes_left > 7 ? 7 : bytes_left) + 1,  g_sent[msg_num].dlc);

        for (int i = 1; i < g_sent[msg_num].dlc; i++) {
            TEST_ASSERT_EQUAL_INT(*build_info_ptr,                    g_sent[msg_num].data[i]);
            bytes_left--;
            if (*build_info_ptr != '\0') {
                build_info_ptr++;
            }
        }
    }

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_REG_VALUE,                       g_sent[msg_count - 1].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_MULTI_MSG_LAST,                  g_sent[msg_count - 1].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                                 g_sent[msg_count - 1].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                                g_sent[msg_count - 1].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(seq,                                        g_sent[msg_count - 1].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(msg_num,                                    g_sent[msg_count - 1].data[0]);

    TEST_ASSERT_EQUAL_INT(msg_num,                                    g_sent[msg_count - 1].data[0]);
    TEST_ASSERT_EQUAL_INT((bytes_left > 7 ? 7 : bytes_left) + 1,      g_sent[msg_count - 1].dlc);

    for (int i = 1; i < g_sent[msg_count - 1].dlc; i++) {
        TEST_ASSERT_EQUAL_INT(*build_info_ptr,                        g_sent[msg_count - 1].data[i]);
        bytes_left--;
        if (*build_info_ptr != '\0') {
            build_info_ptr++;
        }
    }

    TEST_ASSERT_EQUAL_INT(0,                                     bytes_left);
}

void test_std_reg_build_info_write(void) {
    uint8_t tmp[] = "test";
    h9frame_t in = make_set_reg(NODE_BUILD_INFO_STD_REGISTER, tmp, sizeof(tmp), 0);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,          g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,              g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                        g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                       g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(0,                                 g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(H9FRAME_ERROR_READ_ONLY_REGISTER,  g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT(1,                                 g_sent[0].dlc);
}

//NODE_MCU_TYPE_STD_REGISTER
void test_std_reg_mcu_type_read(void) {
    h9frame_t in = make_get_reg(NODE_MCU_TYPE_STD_REGISTER, 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_REG_VALUE,                g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,                g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                          g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                         g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                   g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(NODE_MCU_TYPE_STD_REGISTER,          g_sent[0].data[0]);
    TEST_ASSERT_GREATER_OR_EQUAL_INT8(NODE_MCU_ATMEGA16M1,     g_sent[0].data[1]);
    TEST_ASSERT_LESS_OR_EQUAL_INT8(NODE_MCU_PIC18F46K80,       g_sent[0].data[1]);
    TEST_ASSERT_EQUAL_INT(2,                                   g_sent[0].dlc);
}

void test_std_reg_mcu_type_write(void) {
    uint8_t tmp = NODE_MCU_ATMEGA16C1;
    h9frame_t in = make_set_reg(NODE_MCU_TYPE_STD_REGISTER, &tmp, 1, 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,          g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,              g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                        g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                       g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                 g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(H9FRAME_ERROR_READ_ONLY_REGISTER,  g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT(1,                                 g_sent[0].dlc);
}

//NODE_SN_STD_REGISTER
void test_std_reg_node_sn_read(void) {
    h9frame_t in = make_get_reg(NODE_SN_STD_REGISTER, 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_TRUE(g_sent[0].type == H9FRAME_TYPE_REG_VALUE || g_sent[0].type == H9FRAME_TYPE_COMMAND_ERROR);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,                    g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                              g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                             g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                       g_sent[0].unicast.seqnum);

    if (g_sent[0].type == H9FRAME_TYPE_REG_VALUE) {
        TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_REG_VALUE,                g_sent[0].type);
        TEST_ASSERT_EQUAL_INT(NODE_SN_STD_REGISTER,                g_sent[0].data[0]);
        TEST_ASSERT_EQUAL_INT(5,                                   g_sent[0].dlc);
    }
    else if (g_sent[0].type == H9FRAME_TYPE_COMMAND_ERROR) {
        TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,            g_sent[0].type);
        TEST_ASSERT_EQUAL_INT(H9FRAME_ERROR_UNSUPPORTED_REGISTER,  g_sent[0].data[0]);
        TEST_ASSERT_EQUAL_INT(1,                                   g_sent[0].dlc);
    }
}

void test_std_reg_node_sn_write(void) {
    uint32_t tmp;
    h9frame_t in = make_set_reg(NODE_SN_STD_REGISTER, (uint8_t*)&tmp, sizeof(tmp), 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,          g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,              g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                        g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                       g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                 g_sent[0].unicast.seqnum);
    TEST_ASSERT_TRUE(g_sent[0].data[0] == H9FRAME_ERROR_READ_ONLY_REGISTER || g_sent[0].data[0] == H9FRAME_ERROR_UNSUPPORTED_REGISTER);
    TEST_ASSERT_EQUAL_INT(1,                                 g_sent[0].dlc);
}

//NODE_RESET_REASON_STD_REGISTER
void test_std_reg_reset_reason_read(void) {
    h9frame_t in = make_get_reg(NODE_RESET_REASON_STD_REGISTER, 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_REG_VALUE,                   g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,                   g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                             g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                            g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                      g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(NODE_RESET_REASON_STD_REGISTER,         g_sent[0].data[0]);
    TEST_ASSERT_GREATER_OR_EQUAL_INT8(NODE_RESET_BY_UNKNOWN,       g_sent[0].data[1]);
    TEST_ASSERT_LESS_OR_EQUAL_INT8(NODE_RESET_BY_EXTERNAL_SOURCE, g_sent[0].data[1]);
    TEST_ASSERT_EQUAL_INT(2,                                      g_sent[0].dlc);
}

void test_std_reg_reset_reason_write(void) {
    uint8_t tmp = NODE_RESET_BY_UNKNOWN;
    h9frame_t in = make_set_reg(NODE_RESET_REASON_STD_REGISTER, &tmp, 1, 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,          g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,              g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                        g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                       g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                 g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(H9FRAME_ERROR_READ_ONLY_REGISTER,  g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT(1,                                 g_sent[0].dlc);
}

//NODE_POWER_SUPPLY_STD_REGISTER
void test_std_reg_power_supply_read(void) {
    h9frame_t in = make_get_reg(NODE_POWER_SUPPLY_STD_REGISTER, 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_TRUE(g_sent[0].type == H9FRAME_TYPE_REG_VALUE || g_sent[0].type == H9FRAME_TYPE_COMMAND_ERROR);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,                   g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                             g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                            g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                      g_sent[0].unicast.seqnum);

    if (g_sent[0].type == H9FRAME_TYPE_REG_VALUE) {
        TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_REG_VALUE,               g_sent[0].type);
        TEST_ASSERT_EQUAL_INT(NODE_POWER_SUPPLY_STD_REGISTER,     g_sent[0].data[0]);
        TEST_ASSERT_EQUAL_INT(5,                                  g_sent[0].dlc);
    }
    else if (g_sent[0].type == H9FRAME_TYPE_COMMAND_ERROR) {
        TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,           g_sent[0].type);
        TEST_ASSERT_EQUAL_INT(H9FRAME_ERROR_UNSUPPORTED_REGISTER, g_sent[0].data[0]);
        TEST_ASSERT_EQUAL_INT(1,                                  g_sent[0].dlc);
    }
}

void test_std_reg_power_supply_write(void) {
    uint32_t tmp;
    h9frame_t in = make_set_reg(NODE_POWER_SUPPLY_STD_REGISTER, (uint8_t*)&tmp, sizeof(tmp), 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,          g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,              g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                        g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                       g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                 g_sent[0].unicast.seqnum);
    TEST_ASSERT_TRUE(g_sent[0].data[0] == H9FRAME_ERROR_READ_ONLY_REGISTER || g_sent[0].data[0] == H9FRAME_ERROR_UNSUPPORTED_REGISTER);
    TEST_ASSERT_EQUAL_INT(1,                                 g_sent[0].dlc);
}

//NODE_MCU_TEMP_STD_REGISTER
void test_std_reg_mcu_temp_read(void) {
    h9frame_t in = make_get_reg(NODE_MCU_TEMP_STD_REGISTER, 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_TRUE(g_sent[0].type == H9FRAME_TYPE_REG_VALUE || g_sent[0].type == H9FRAME_TYPE_COMMAND_ERROR);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,                   g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                             g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                            g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                      g_sent[0].unicast.seqnum);

    if (g_sent[0].type == H9FRAME_TYPE_REG_VALUE) {
        TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_REG_VALUE,               g_sent[0].type);
        TEST_ASSERT_EQUAL_INT(NODE_MCU_TEMP_STD_REGISTER,         g_sent[0].data[0]);
        TEST_ASSERT_EQUAL_INT(5,                                  g_sent[0].dlc);
    }
    else if (g_sent[0].type == H9FRAME_TYPE_COMMAND_ERROR) {
        TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,           g_sent[0].type);
        TEST_ASSERT_EQUAL_INT(H9FRAME_ERROR_UNSUPPORTED_REGISTER, g_sent[0].data[0]);
        TEST_ASSERT_EQUAL_INT(1,                                  g_sent[0].dlc);
    }
}

void test_std_reg_mcu_temp_write(void) {
    uint32_t tmp;
    h9frame_t in = make_set_reg(NODE_MCU_TEMP_STD_REGISTER, (uint8_t*)&tmp, sizeof(tmp), 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,          g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,              g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                        g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                       g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                 g_sent[0].unicast.seqnum);
    TEST_ASSERT_TRUE(g_sent[0].data[0] == H9FRAME_ERROR_READ_ONLY_REGISTER || g_sent[0].data[0] == H9FRAME_ERROR_UNSUPPORTED_REGISTER);
    TEST_ASSERT_EQUAL_INT(1,                                 g_sent[0].dlc);
}

//NODE_ID_STD_REGISTER
void test_std_reg_node_id_read(void) {
    h9frame_t in = make_get_reg(NODE_ID_STD_REGISTER, 7);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_REG_VALUE,              g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,              g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                        g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                       g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(7,                                 g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(NODE_ID_STD_REGISTER,              g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT((MY_NODE_ID >> 8) & 0xff,          g_sent[0].data[1]);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID & 0xff,                 g_sent[0].data[2]);
    TEST_ASSERT_EQUAL_INT(3,                                 g_sent[0].dlc);
}

void test_std_reg_node_id_write(void) {
    uint16_t tmp = MY_NODE_ID + 1;
    h9frame_t in = make_set_reg(NODE_ID_STD_REGISTER, (uint8_t*)&tmp, 2, 0);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_REG_VALUE,              g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,              g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                        g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                       g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(0,                                 g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(NODE_ID_STD_REGISTER,              g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT((MY_NODE_ID >> 8) & 0xff,          g_sent[0].data[1]);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID & 0xff,                 g_sent[0].data[2]);
    TEST_ASSERT_EQUAL_INT(3,                                 g_sent[0].dlc);
}

void test_std_reg_node_id_oversize_write(void) {
    uint32_t tmp = MY_NODE_ID + 1;
    h9frame_t in = make_set_reg(NODE_ID_STD_REGISTER, (uint8_t*)&tmp, 4, 0);
    TEST_ASSERT_EQUAL_UINT8(0, process_msg(&in));

    TEST_ASSERT_EQUAL_INT(1, g_send_count);

    TEST_ASSERT_EQUAL_INT(H9FRAME_TYPE_COMMAND_ERROR,             g_sent[0].type);
    TEST_ASSERT_EQUAL_INT(H9FRAME_FLAG_SINGE_MSG,                 g_sent[0].unicast.flags);
    TEST_ASSERT_EQUAL_INT(MY_NODE_ID,                           g_sent[0].source_id);
    TEST_ASSERT_EQUAL_INT(TEST_SRC_ID,                          g_sent[0].unicast.destination_id);
    TEST_ASSERT_EQUAL_INT(0,                                    g_sent[0].unicast.seqnum);
    TEST_ASSERT_EQUAL_INT(H9FRAME_ERROR_REGISTER_SIZE_MISMATCH, g_sent[0].data[0]);
    TEST_ASSERT_EQUAL_INT(1,                                    g_sent[0].dlc);
}

/* ------------------------------------------------------------------ */

int main(void) {
    CAN_init(MY_NODE_TYPE, MY_HARDWARE_REV, MY_VERSION_MAJOR, MY_VERSION_MINOR, MY_BUILD_INFO);
    can_node_id = MY_NODE_ID;

    UNITY_BEGIN();
    RUN_TEST(test_non_standard_reg_returned_to_app);

    RUN_TEST(test_bootloader_type_msg);
    RUN_TEST(test_unicast_dst_id_mismatch);
    RUN_TEST(test_broadcast_group_mismatch_discover);
    RUN_TEST(test_broadcast_group_mismatch_reset);
    RUN_TEST(test_broadcast_group_mismatch_reg_value);

    RUN_TEST(test_broadcast_group_discover);
    RUN_TEST(test_broadcast_group_reset);
    RUN_TEST(test_broadcast_group_all_discover);
    RUN_TEST(test_broadcast_group_all_reset);

    RUN_TEST(test_set_bit_on_readonly);
    RUN_TEST(test_set_bit_on_writable);

    //dev standard reg
    RUN_TEST(test_std_reg_node_type_read);
    RUN_TEST(test_std_reg_node_type_write);
    RUN_TEST(test_std_reg_node_hardware_revision_read);
    RUN_TEST(test_std_reg_node_hardware_revision_write);
    RUN_TEST(test_std_reg_node_versio_read);
    RUN_TEST(test_std_reg_node_versio_write);
    RUN_TEST(test_std_reg_build_info_read);
    RUN_TEST(test_std_reg_build_info_write);
    RUN_TEST(test_std_reg_mcu_type_read);
    RUN_TEST(test_std_reg_mcu_type_write);
    RUN_TEST(test_std_reg_node_sn_read);
    RUN_TEST(test_std_reg_node_sn_write);
    RUN_TEST(test_std_reg_reset_reason_read);
    RUN_TEST(test_std_reg_reset_reason_write);
    RUN_TEST(test_std_reg_power_supply_read);
    RUN_TEST(test_std_reg_power_supply_write);
    RUN_TEST(test_std_reg_mcu_temp_read);
    RUN_TEST(test_std_reg_mcu_temp_write);
    RUN_TEST(test_std_reg_node_id_read);
    RUN_TEST(test_std_reg_node_id_write);
    RUN_TEST(test_std_reg_node_id_oversize_write);
    return UNITY_END();
}
