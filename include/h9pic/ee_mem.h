#ifndef EE_MEM_H
#define EE_MEM_H

#include <xc.h>

typedef struct {
    uint8_t magicbyte;
    uint8_t counter;
    uint8_t node_id;
    uint8_t crc;
} node_can_setting_t;

#define MAGIC_BYTE       0xA5
#define SECTOR_NUMBER    16u
#define USER_BASE        0x80

uint8_t read_node_id(void);
uint8_t read_node_id_and_refresh(void);
void write_node_id(uint8_t id);

void eeprom_write_byte(uint16_t addr, uint8_t data);
uint8_t eeprom_read_byte(uint16_t addr);

uint8_t read_data_and_refresh(uint16_t addr_base, uint8_t *data, uint8_t size);
void write_data(uint16_t addr_base, uint8_t *data, uint8_t size);

#endif
