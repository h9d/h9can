#include "h9pic/ee_mem.h"

#define ROUND_UP_4(x)          (((x) + 3u) & ~3u)
#define SECTOR_ADDRES(base, offset, size) (base + (((offset * 3) & 0x0f) * (ROUND_UP_4(size) + 4)))
#define COUNTER_VALID_WINDOW  (128u - SECTOR_NUMBER)   /* 112 */

void eeprom_write_byte(uint16_t addr, uint8_t data) {
    uint8_t GIEBitValue = INTCONbits.GIE;
    EEADRH = ((addr >> 8) & 0x03);
    EEADR = (addr & 0xFF);
    EEDATA = data;
    EECON1bits.EEPGD = 0;
    EECON1bits.CFGS = 0;
    EECON1bits.WREN = 1;
    INTCONbits.GIE = 0;     // Disable interrupts
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1;
    // Wait for write to complete
    while (EECON1bits.WR);
    EECON1bits.WREN = 0;
    INTCONbits.GIE = GIEBitValue;   // restore interrupt enable
}

uint8_t eeprom_read_byte(uint16_t addr) {
    EEADRH = ((addr >> 8) & 0x03);
    EEADR = (addr & 0xFF);
    EECON1bits.CFGS = 0;
    EECON1bits.EEPGD = 0;
    EECON1bits.RD = 1;
    NOP();  // NOPs may be required for latency at high frequencies
    NOP();

    return (EEDATA);
}

////CRC-8/MAXIM
//static uint8_t crc8_update(uint8_t crc, uint8_t byte) {
//    crc ^= byte;
//    uint8_t i;
//    for (i = 0; i < 8u; i++) {
//        if (crc & 0x80u) crc = ((uint8_t)(crc << 1u) ^ 0x31u);
//        else crc = (uint8_t)(crc << 1u);
//    }
//    return crc;
//}
//
//uint8_t crc_sum(node_can_setting_t* sector) {
//    uint8_t crc = 0x00u;
//    crc = crc8_update(crc, sector->magicbyte);
//    crc = crc8_update(crc, sector->counter);
//    crc = crc8_update(crc, sector->node_id);
//    return crc;
//}
//
//void read_sector(uint8_t idx, node_can_setting_t *buf) {
//    uint16_t addr = idx * sizeof(node_can_setting_t);
//    buf->magicbyte = eeprom_read_byte(addr);
//    buf->counter = eeprom_read_byte(addr+1);
//    buf->node_id = eeprom_read_byte(addr+2);
//    buf->crc = eeprom_read_byte(addr+3);
//}
//
//void write_sector(uint8_t idx, node_can_setting_t *buf) {
//    uint16_t addr = idx * sizeof(node_can_setting_t);
//    eeprom_write_byte(addr + 0, buf->magicbyte);
//    eeprom_write_byte(addr + 1, buf->counter);
//    eeprom_write_byte(addr + 2, buf->node_id);
//    eeprom_write_byte(addr + 3, buf->crc);
//}
//
static uint8_t cyclic_counter_great(uint8_t a, uint8_t b) {
    //if (a == b) return 0;

    /*
     * Liczymy roznice (b - a) mod 256.
     * Odejmowanie uint8_t naturalnie daje wynik mod 256 w C
     * (zachowanie wraparound dla typow bez znaku jest gwarantowane
     *  przez standard C99 section 6.2.5).
     *
     * Przyklady:
     *   a=253, b=3  -> diff = (3-253) mod 256 = 6   -> b nowszy o 6
     *   a=3, b=253  -> diff = (253-3) mod 256 = 250  -> a nowszy o 6
     *   a=100, b=5  -> diff = (5-100) mod 256 = 161  -> strefa martwa
     */
    uint8_t diff = (uint8_t)(b - a);

    //if (diff <= COUNTER_VALID_WINDOW) {
    //    /* diff w [1, 112]: b jest nowszy o 'diff' krokow */
    //    return 0;
    //}

    if (diff >= (uint8_t)(256u - COUNTER_VALID_WINDOW)) {
        /* diff w [144, 255]: a jest nowszy o (256-diff) krokow */
        return 1;
    }

    /* diff w [113, 143]: strefa martwa - nie rozstrzygamy */
    return 0;
}
//
//static uint8_t write_node_and_verify(uint8_t idx, uint8_t id, uint8_t counter) {
//    node_can_setting_t new_setting;
//    new_setting.magicbyte = MAGIC_BYTE;
//    new_setting.counter = counter;
//    new_setting.node_id = id;
//    new_setting.crc = crc_sum(&new_setting);
//    write_sector((idx * 3) & 0x0f, &new_setting);
//
//    node_can_setting_t test;
//    read_sector((idx * 3) & 0x0f, &test);
//    if (test.magicbyte == MAGIC_BYTE && test.counter == new_setting.counter &&  test.crc == new_setting.crc && test.node_id == new_setting.node_id) {
//        return 1;
//    }
//    return 0;
//}
//
//static void refresh_bad_call(uint8_t id, uint8_t last_counter) {
//    node_can_setting_t tmp;
//
//    for (uint8_t i = 0; i < SECTOR_NUMBER; ++i) {
//        read_sector((i * 3) & 0x0f, &tmp);
//        if (tmp.magicbyte != MAGIC_BYTE || tmp.crc != crc_sum(&tmp)) {
//            if (write_node_and_verify(i, id, last_counter + 1))
//                break;
//        }
//    }
//}
//
//uint8_t read_node_id(void) {
//    node_can_setting_t max;
//    int i = 0;
//    for (; i < SECTOR_NUMBER; ++i) {
//        read_sector((i * 3) & 0x0f, &max);
//        if (max.magicbyte == MAGIC_BYTE && max.crc == crc_sum(&max)) {
//            break;
//        }
//    }
//
//    if (i < SECTOR_NUMBER) {
//        i++;
//        for (; i < SECTOR_NUMBER; ++i) {
//            node_can_setting_t buf;
//            read_sector((i * 3) & 0x0f, &buf);
//            if (buf.magicbyte == MAGIC_BYTE && buf.crc == crc_sum(&buf) && cyclic_counter_great(buf.counter, max.counter)) {
//                 max = buf;
//            }
//        }
//        return max.node_id;
//    }
//
//    return 0xff; //pamiec pusta
//}
//
//uint8_t read_node_id_and_refresh(void){
//    node_can_setting_t max;
//    int i = 0;
//    int error_cell_count = 0;
//    for (; i < SECTOR_NUMBER; ++i) {
//        read_sector((i * 3) & 0x0f, &max);
//        if (max.magicbyte == MAGIC_BYTE && max.crc == crc_sum(&max)) {
//            break;
//        }
//        error_cell_count++;
//    }
//
//    if (i < SECTOR_NUMBER) {
//        i++;
//        for (; i < SECTOR_NUMBER; ++i) {
//            node_can_setting_t buf;
//            read_sector((i * 3) & 0x0f, &buf);
//            if (buf.magicbyte == MAGIC_BYTE && buf.crc == crc_sum(&buf)) {
//                if (cyclic_counter_great(buf.counter, max.counter))
//                    max = buf;
//            }
//            else {
//                error_cell_count++;
//            }
//        }
//        if (error_cell_count) {
//            refresh_bad_call(max.node_id, max.counter);
//        }
//        return max.node_id;
//    }
//
//    return 0xff; //pamiec pusta
//}
//
//void write_node_id(uint8_t id) {
//    node_can_setting_t max;
//    int max_idx = -1;
//    int i = 0;
//    for (; i < SECTOR_NUMBER; ++i) {
//        read_sector((i * 3) & 0x0f, &max);
//        if (max.magicbyte == MAGIC_BYTE && max.crc == crc_sum(&max)) {
//            max_idx = i;
//            break;
//        }
//    }
//    if (i < SECTOR_NUMBER) {
//        i++;
//        for (; i < SECTOR_NUMBER; ++i) {
//            node_can_setting_t buf;
//            read_sector((i * 3) & 0x0f, &buf);
//            if (buf.magicbyte == MAGIC_BYTE && cyclic_counter_great(buf.counter, max.counter) && buf.crc == crc_sum(&buf)) {
//                max_idx = i;
//                max = buf;
//            }
//        }
//    }
//    int j = 0;
//    for (; j < SECTOR_NUMBER; ++j) {
//        if (write_node_and_verify((uint8_t)(max_idx + 1 + j), id, (max_idx > -1) ? max.counter + 1 : 0))
//            break;
//    }
//
//    j++;
//    for (; j < SECTOR_NUMBER; ++j) {
//        if (write_node_and_verify((uint8_t)(max_idx + 1 + j), id, (max_idx > -1) ? max.counter + 2 : 1))
//            break;
//    }
//}

//CRC-16/CCITT
static uint16_t crc16_update(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)((uint16_t)byte << 8);
    uint8_t i;
    for (i = 0; i < 8u; i++) {
        if (crc & 0x8000u) crc = ((uint16_t)(crc << 1u) ^ 0x1021u);
        else crc = (uint16_t)(crc << 1u);
    }
    return crc;
}
 
static uint16_t crc16_sum(uint8_t counter, const uint8_t *data, uint8_t size) {
    uint16_t crc = 0xFFFFu;
    crc = crc16_update(crc, MAGIC_BYTE);
    crc = crc16_update(crc, counter);
    uint8_t i;
    for (i = 0; i < size; i++) crc = crc16_update(crc, data[i]);
    return crc;
}

static uint8_t read_sector(uint16_t addr, uint8_t *counter, uint8_t *data, uint8_t size) {
    uint16_t crc = 0xFFFFu;
    
    if (eeprom_read_byte(addr) != MAGIC_BYTE) return 0;
    crc = crc16_update(crc, MAGIC_BYTE);
    
    *counter = eeprom_read_byte(addr + 1);
    crc = crc16_update(crc, *counter);
    
    for (uint8_t i = 0; i < size; ++i) {
        uint8_t tmp = eeprom_read_byte(addr + 2 + i);
        crc = crc16_update(crc, tmp);
        if (data) data[i] = tmp;
    }
    
    uint16_t r_crc = eeprom_read_byte(addr + 2 + size);
    r_crc <<= 8;
    r_crc |= eeprom_read_byte(addr + 3 + size);
    
    return r_crc == crc;
}

static uint8_t write_sector_and_verify(uint16_t addr, uint8_t counter, uint8_t *data, uint8_t size) {
    //write
    uint16_t crc = crc16_sum(counter, data, size);
    
    eeprom_write_byte(addr + 0, MAGIC_BYTE);
    eeprom_write_byte(addr + 1, counter);
    for (uint8_t i =0; i < size; ++i) {
        eeprom_write_byte(addr + 2 + i, data[i]);
    }
    eeprom_write_byte(addr + 2 + size, (uint8_t)(crc >> 8));
    eeprom_write_byte(addr + 3 + size, (uint8_t)(crc));
    
    //verify
    if (eeprom_read_byte(addr) != MAGIC_BYTE) return 0;
    if (eeprom_read_byte(addr + 1) != counter) return 0;
    for (uint8_t i =0; i < size; ++i) {
        if (eeprom_read_byte(addr + 2 + i) != data[i]) return 0;
    }
    if (eeprom_read_byte(addr + 2 + size) != (uint8_t)(crc >> 8)) return 0;
    if (eeprom_read_byte(addr + 3 + size) != (uint8_t)(crc)) return 0;
    return 1;
}

static void refresh_bad_sector(uint16_t addr_base, uint8_t counter, uint8_t *data, uint8_t size) {
    uint8_t tmp_counter;

    for (uint8_t i = 0; i < SECTOR_NUMBER; ++i) {
        if (!read_sector(SECTOR_ADDRES(addr_base, i, size), &tmp_counter, NULL, size)) {
            if (write_sector_and_verify(SECTOR_ADDRES(addr_base, i, size), counter + 1, data, size))
                break;
        }
    }
}

uint8_t read_data_and_refresh(uint16_t addr_base, uint8_t *data, uint8_t size) {
    uint8_t max_counter;
    uint8_t max_idx;
    
    uint8_t i = 0;
    uint8_t error_cell_count = 0;
    for (; i < SECTOR_NUMBER; ++i) {
        if (read_sector(SECTOR_ADDRES(addr_base, i, size), &max_counter, NULL, size)) {
            max_idx = i;
            break;
        }
        error_cell_count++;
    }

    if (i < SECTOR_NUMBER) {
        i++;
        for (; i < SECTOR_NUMBER; ++i) {
            uint8_t tmp_counter;
            if (read_sector(SECTOR_ADDRES(addr_base, i, size), &tmp_counter, NULL, size)) {
                if (cyclic_counter_great(tmp_counter, max_counter)) {
                    max_counter = tmp_counter;
                    max_idx = i;
                }
            }
            else {
                error_cell_count++;
            }
        }
        
        if (read_sector(SECTOR_ADDRES(addr_base, max_idx, size), &max_counter, data, size)) { 
            if (error_cell_count) {
                refresh_bad_sector(addr_base, max_counter, data, size);
            }
            return 1;
        }
    }

    return 0; //pamiec pusta
}

void write_data(uint16_t addr_base, uint8_t *data, uint8_t size) {
    uint8_t max_counter;
    int max_idx = -1;
    int i = 0;
    for (; i < SECTOR_NUMBER; ++i) {
        if (read_sector(SECTOR_ADDRES(addr_base, i, size), &max_counter, NULL, size)) {
            max_idx = i;
            break;
        }
    }
    if (i < SECTOR_NUMBER) {
        i++;
        for (; i < SECTOR_NUMBER; ++i) {
            uint8_t tmp_counter;
            if (read_sector(SECTOR_ADDRES(addr_base, i, size), &tmp_counter, NULL, size)) {
                if (cyclic_counter_great(tmp_counter, max_counter)) {
                    max_counter = tmp_counter;
                    max_idx = i;
                }
            }
        }
    }
    int j = 0;
    for (; j < SECTOR_NUMBER; ++j) {
        
        if (write_sector_and_verify(SECTOR_ADDRES(addr_base, max_idx + 1 + j, size), (max_idx > -1) ? max_counter + 1 : 0, data,  size))
            break;
    }

    j++;
    for (; j < SECTOR_NUMBER; ++j) {
        if (write_sector_and_verify(SECTOR_ADDRES(addr_base, max_idx + 1 + j, size), (max_idx > -1) ? max_counter + 2 : 1, data,  size))
            break;
    }
}

uint8_t read_node_id_and_refresh(void){
    uint8_t id;
    if (read_data_and_refresh(0, &id, sizeof(uint8_t)))
        return id;
    return 0xff;
}

void write_node_id(uint8_t id) {
    write_data(0, &id, sizeof(uint8_t));
}
