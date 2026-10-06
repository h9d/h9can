#ifndef EE_MEM_H
#define EE_MEM_H

#include <xc.h>

//typedef struct {
//    uint8_t magicbyte;
//    uint8_t counter;
//    uint8_t node_id;
//    uint8_t crc;
//} node_can_setting_t;

#define MAGIC_BYTE       0xA5
#define SECTOR_NUMBER    16u        // must be 16: SECTOR_ADDRES interleaves sectors with (offset * 3) & 0x0f
#define USER_BASE        0x80       // application data (0x80-...), node id: h9pic/node_id.h


//void eeprom_write_byte(uint16_t addr, uint8_t data);
//uint8_t eeprom_read_byte(uint16_t addr);

uint8_t read_data(uint16_t addr_base, uint8_t *data, uint8_t size);
uint8_t read_data_and_refresh(uint16_t addr_base, uint8_t *data, uint8_t size);
void write_data(uint16_t addr_base, uint8_t *data, uint8_t size);

#endif
