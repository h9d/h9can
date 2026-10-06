/*
 * Created by crowx on 29/05/2026.
 *
 * Node id storage in EEPROM.
 *
 * The id is kept in two independent copies (rings far apart in EEPROM), each a ring of
 * EEPROM_NODE_ID_BLOCK_COUNT blocks (wear leveling). Every write puts the same block (same sequence
 * number) into the next slot of both rings, then invalidates all other blocks. Reading picks the valid
 * block (flags + CRC) with the newest sequence number from either copy, so:
 *  - an interrupted write (power loss) still returns the newest complete id,
 *  - a damaged EEPROM page loses at most one block of one copy; the other copy is used,
 *    and node_id_repair() restores the damaged copy.
 *
 * A block is 8 bytes aligned to 8, i.e. exactly two whole EEPROM pages (4 bytes on ATmega16/32/64M1),
 * so one damaged page never affects two blocks.
 */

#ifndef H9AVR_NODE_ID_H
#define H9AVR_NODE_ID_H

#include <stddef.h>
#include <avr/io.h>
#include <avr/eeprom.h>

typedef struct {
    uint8_t flags;
    uint8_t seq;            // write sequence number (wraps), the newest valid block wins
    uint8_t node_id;
    uint8_t reserved;       // 0xff, pads the block to 8 bytes (two EEPROM pages)
    uint16_t node_type;
    uint16_t crc;           // CRC-16/CCITT of the fields above
} h9node_id_t;

#define EEPROM_BLOCK_VALID 0xaa
#define EEPROM_BLOCK_INVALID 0x00
#define EEPROM_NODE_ID_BLOCK_COUNT 10
#define EEPROM_NODE_ID_COPIES 2

// Address 0 is the most exposed to corruption: EEAR resets to 0, so a stray write during brown-out /
// power-up hits it. Keep the beginning of EEPROM unused; the two copies are placed far apart.
// Both rings fit in the smallest EEPROM of the family (512 bytes): 0x10-0x5f and 0x80-0xcf.
#define EEPROM_NODE_ID_COPY0_OFFSET 0x10
#define EEPROM_NODE_ID_COPY1_OFFSET 0x80

#define EEPROM_NODE_ID_BLOCK_ADDR(copy, i) \
    ((void *)(((copy) ? EEPROM_NODE_ID_COPY1_OFFSET : EEPROM_NODE_ID_COPY0_OFFSET) + sizeof(h9node_id_t) * (i)))

_Static_assert(sizeof(h9node_id_t) == 8, "h9node_id_t must be 8 bytes (two EEPROM pages)");
_Static_assert(EEPROM_NODE_ID_COPY0_OFFSET + 8 * EEPROM_NODE_ID_BLOCK_COUNT <= EEPROM_NODE_ID_COPY1_OFFSET, "node id copies overlap");

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

static uint8_t node_id_block_read(uint8_t copy, uint8_t i, h9node_id_t *b) {
    eeprom_read_block(b, EEPROM_NODE_ID_BLOCK_ADDR(copy, i), sizeof(*b));
    return b->flags == EEPROM_BLOCK_VALID && b->crc == crc16(b, offsetof(h9node_id_t, crc));
}

// Find the valid block with the newest sequence number in either copy; returns its slot index or -1 if none.
static int8_t find_node_id_block(h9node_id_t *out) {
    int8_t newest = -1;
    h9node_id_t b;

    for (uint8_t copy = 0; copy < EEPROM_NODE_ID_COPIES; copy++) {
        for (uint8_t i = 0; i < EEPROM_NODE_ID_BLOCK_COUNT; i++) {
            if (!node_id_block_read(copy, i, &b))
                continue;

            // wrapping sequence number: newer if (seq - newest_seq) mod 256 is in 1..127
            uint8_t diff = (uint8_t)(b.seq - out->seq);
            if (newest < 0 || (diff != 0 && diff < 128)) {
                *out = b;
                newest = (int8_t)i;
            }
        }
    }

    return newest;
}

// Returns the stored node id, or 0 if there is no valid one.
static uint8_t read_node_id(void) {
    h9node_id_t b;

    if (find_node_id_block(&b) < 0)
        return 0;

    return b.node_id;
}

static __attribute__((unused)) void node_id_store(uint8_t slot, const h9node_id_t *b) {
    for (uint8_t copy = 0; copy < EEPROM_NODE_ID_COPIES; copy++)
        eeprom_update_block(b, EEPROM_NODE_ID_BLOCK_ADDR(copy, slot), sizeof(*b));

    // invalidate all other blocks in both copies (one byte each, only written if it changes), including
    // stale ones left by an earlier interrupted write; if this is interrupted the newer seq still wins
    for (uint8_t copy = 0; copy < EEPROM_NODE_ID_COPIES; copy++) {
        for (uint8_t i = 0; i < EEPROM_NODE_ID_BLOCK_COUNT; i++) {
            if (i != slot)
                eeprom_update_byte((uint8_t *)EEPROM_NODE_ID_BLOCK_ADDR(copy, i) + offsetof(h9node_id_t, flags), EEPROM_BLOCK_INVALID);
        }
    }
}

static __attribute__((unused)) void write_node_id(uint8_t node_id, uint16_t node_type) {
    h9node_id_t b;
    int8_t current = find_node_id_block(&b);

    uint8_t slot = current < 0 ? 0 : (uint8_t)((current + 1) % EEPROM_NODE_ID_BLOCK_COUNT);
    uint8_t seq = current < 0 ? 0 : (uint8_t)(b.seq + 1);

    b.flags = EEPROM_BLOCK_VALID;
    b.seq = seq;
    b.node_id = node_id;
    b.reserved = 0xff;
    b.node_type = node_type;
    b.crc = crc16(&b, offsetof(h9node_id_t, crc));
    node_id_store(slot, &b);
}

/**
 * @brief Restore a damaged copy from the good one.
 *
 * Writes EEPROM only if the newest block is not valid in both copies (or stale blocks are left),
 * so a normal start does not write anything.
 * @retval 1  A copy was repaired.
 * @retval 0  Nothing to repair (both copies fine, or no id stored at all).
 */
static __attribute__((unused)) uint8_t node_id_repair(void) {
    h9node_id_t newest, b;
    int8_t slot = find_node_id_block(&newest);
    if (slot < 0)
        return 0;

    uint8_t damaged = 0;
    for (uint8_t copy = 0; copy < EEPROM_NODE_ID_COPIES; copy++) {
        for (uint8_t i = 0; i < EEPROM_NODE_ID_BLOCK_COUNT; i++) {
            uint8_t valid = node_id_block_read(copy, i, &b);
            if (i == (uint8_t)slot ? (!valid || b.seq != newest.seq) : valid)
                damaged = 1;
        }
    }

    if (damaged)
        node_id_store((uint8_t)slot, &newest);
    return damaged;
}

#endif //H9AVR_NODE_ID_H
