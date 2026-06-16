#pragma once
#include <stdint.h>

/* Treat the EEPROM address as a regular pointer into the BSS variable.
   ee_node_id is declared 'static uint8_t ee_node_id = 0' (the section attribute
   is removed when compiling without __AVR__), so pointer arithmetic is valid. */
static inline uint8_t eeprom_read_byte(const uint8_t *addr)   { return *addr; }
static inline void    eeprom_write_byte(uint8_t *addr, uint8_t val) { *addr = val; }
static inline void    eeprom_update_byte(uint8_t *addr, uint8_t val) { *addr = val; }
