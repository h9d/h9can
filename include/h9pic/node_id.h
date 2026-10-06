/*
 * Node id storage in EEPROM (PIC), same API as h9avr/node_id.h.
 *
 * The id is kept by the ee_mem sector mechanism: 16 interleaved sectors, every write stores the id in two
 * sectors (consecutive counters) and invalidates all other ones; the valid sector with the newest counter
 * is used, so an interrupted write or a damaged sector never loses the id.
 */

#ifndef H9PIC_NODE_ID_H
#define H9PIC_NODE_ID_H

#include <xc.h>
#include "h9pic/ee_mem.h"

// node id sectors 0x100-0x17f; 0x00-0x7f unused (address 0 is the most exposed to corruption)
#define NODE_ID_BASE     0x100

// Returns the stored node id, or 0 if there is no valid one.
static __attribute__((unused)) uint8_t read_node_id(void) {
    uint8_t id;
    if (read_data(NODE_ID_BASE, &id, sizeof(id)))
        return id;
    return 0;
}

static __attribute__((unused)) void write_node_id(uint8_t node_id) {
    write_data(NODE_ID_BASE, &node_id, sizeof(node_id));
}

// Restore a damaged copy from the good one (writes EEPROM only if needed).
static __attribute__((unused)) void node_id_repair(void) {
    uint8_t id;
    read_data_and_refresh(NODE_ID_BASE, &id, sizeof(id));
}

#endif /* H9PIC_NODE_ID_H */
