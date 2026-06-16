/*
 * Created by crowx on 29/05/2026.
 *
 */

#ifndef H9AVR_NODE_ID_H
#define H9AVR_NODE_ID_H

#include <avr/io.h>
#include <avr/eeprom.h>

typedef struct {
    uint8_t flags;
    uint8_t node_id;
    uint16_t node_type;
    uint16_t crc;
} h9node_id_t;

#define EEPROM_BLOCK_VALID 0xaa
#define EEPROM_NODE_ID_BLOCK_COUNT 10

static uint16_t crc16(const void* data, size_t length) {
    const uint8_t* bytes = data;
    uint16_t crc = 0xFFFF;

    while (length--) {
        crc ^= (uint16_t)(*bytes++) << 8;

        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }

    return crc;
}

static uint8_t read_node_id(uint16_t* node_type) {
    h9node_id_t id_struct;

    for (uint8_t i = 0; i < EEPROM_NODE_ID_BLOCK_COUNT; i++) {
        eeprom_read_block(&id_struct, (void*)(sizeof(id_struct) * i), sizeof(id_struct));

        if (id_struct.crc == crc16(&id_struct, offsetof(h9node_id_t, crc)) && id_struct.flags == EEPROM_BLOCK_VALID) {
            if (node_type) {
                *node_type = id_struct.node_type;
            }
            return id_struct.node_id;
        }
    }

    return 0;
}

static void write_node_id(uint8_t node_id, uint16_t node_type) {
    h9node_id_t id_struct;

    uint8_t i = 0;
    for (i = 0; i < EEPROM_NODE_ID_BLOCK_COUNT; i++) {
        eeprom_read_block(&id_struct, (void*)(sizeof(id_struct) * i), sizeof(id_struct));

        if (id_struct.crc == crc16(&id_struct, offsetof(h9node_id_t, crc)) && id_struct.flags == EEPROM_BLOCK_VALID) {
            break;
        }
    }
    uint8_t next_block = i == EEPROM_NODE_ID_BLOCK_COUNT ? 0 : i + 1;
    next_block %= EEPROM_NODE_ID_BLOCK_COUNT;

    id_struct.flags = EEPROM_BLOCK_VALID;
    id_struct.node_id = node_id;
    id_struct.node_type = node_type;
    id_struct.crc = crc16(&id_struct, offsetof(h9node_id_t, crc));
    eeprom_write_block(&id_struct, (void*)(sizeof(id_struct) * next_block), sizeof(id_struct));

    if (i != EEPROM_NODE_ID_BLOCK_COUNT) {
        id_struct.flags = 0xff;
        id_struct.crc = crc16(&id_struct, offsetof(h9node_id_t, crc));
        eeprom_write_block(&id_struct, (void*)(sizeof(id_struct) * i), sizeof(id_struct));
    }
}

#endif //H9AVR_NODE_ID_H