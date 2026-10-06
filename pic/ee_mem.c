#include "h9pic/ee_mem.h"

#define ROUND_UP_4(x)          (((x) + 3u) & ~3u)
#define SECTOR_ADDRES(base, offset, size) (base + (((offset * 3) & 0x0f) * (ROUND_UP_4(size) + 4)))
#define COUNTER_VALID_WINDOW  (128u - SECTOR_NUMBER)   /* 112 */

static void eeprom_write_byte(uint16_t addr, uint8_t data) {
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

static uint8_t eeprom_read_byte(uint16_t addr) {
    EEADRH = ((addr >> 8) & 0x03);
    EEADR = (addr & 0xFF);
    EECON1bits.CFGS = 0;
    EECON1bits.EEPGD = 0;
    EECON1bits.RD = 1;
    NOP();  // NOPs may be required for latency at high frequencies
    NOP();

    return (EEDATA);
}

// write only if the value changes (less wear)
static void eeprom_update_byte(uint16_t addr, uint8_t data) {
    if (eeprom_read_byte(addr) != data)
        eeprom_write_byte(addr, data);
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
    
    eeprom_update_byte(addr + 0, MAGIC_BYTE);
    eeprom_update_byte(addr + 1, counter);
    for (uint8_t i =0; i < size; ++i) {
        eeprom_update_byte(addr + 2 + i, data[i]);
    }
    eeprom_update_byte(addr + 2 + size, (uint8_t)(crc >> 8));
    eeprom_update_byte(addr + 3 + size, (uint8_t)(crc));
    
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

/*
 * Every value (node id, user data) is kept in SECTOR_NUMBER sectors at addr_base. A write stores the value
 * twice (two sectors, counters n and n + 1) and then invalidates all other sectors, so exactly two valid
 * copies of the newest value exist. Reading picks the valid sector (magic + CRC) with the newest counter;
 * read_data_and_refresh() rewrites the two copies only if one of them is missing / damaged or stale
 * sectors are left (e.g. power loss during a write), a healthy memory is never written at start.
 */

// newest valid sector: returns its index (or -1 if none) and its counter; valid_count = all valid sectors
static int8_t find_newest_sector(uint16_t addr_base, uint8_t size, uint8_t *counter, uint8_t *valid_count) {
    int8_t newest = -1;
    uint8_t valid = 0;

    for (uint8_t i = 0; i < SECTOR_NUMBER; ++i) {
        uint8_t tmp_counter;
        if (read_sector(SECTOR_ADDRES(addr_base, i, size), &tmp_counter, NULL, size)) {
            valid++;
            if (newest < 0 || cyclic_counter_great(tmp_counter, *counter)) {
                *counter = tmp_counter;
                newest = (int8_t)i;
            }
        }
    }

    *valid_count = valid;
    return newest;
}

uint8_t read_data(uint16_t addr_base, uint8_t *data, uint8_t size) {
    uint8_t counter;
    uint8_t valid;
    int8_t newest = find_newest_sector(addr_base, size, &counter, &valid);

    if (newest < 0)
        return 0; //pamiec pusta

    return read_sector(SECTOR_ADDRES(addr_base, (uint8_t)newest, size), &counter, data, size);
}

uint8_t read_data_and_refresh(uint16_t addr_base, uint8_t *data, uint8_t size) {
    uint8_t counter;
    uint8_t valid;
    int8_t newest = find_newest_sector(addr_base, size, &counter, &valid);

    if (newest < 0 || !read_sector(SECTOR_ADDRES(addr_base, (uint8_t)newest, size), &counter, data, size))
        return 0; //pamiec pusta

    // healthy: exactly two valid sectors, the second one holds the same data with counter - 1
    uint8_t healthy = 0;
    if (valid == 2) {
        uint16_t newest_addr = SECTOR_ADDRES(addr_base, (uint8_t)newest, size);
        for (uint8_t i = 0; i < SECTOR_NUMBER; ++i) {
            uint16_t addr = SECTOR_ADDRES(addr_base, i, size);
            uint8_t tmp_counter;
            if (i == (uint8_t)newest || !read_sector(addr, &tmp_counter, NULL, size))
                continue;
            if (tmp_counter == (uint8_t)(counter - 1)) {
                healthy = 1;
                for (uint8_t k = 0; k < size; ++k) {
                    if (eeprom_read_byte(addr + 2 + k) != eeprom_read_byte(newest_addr + 2 + k))
                        healthy = 0;
                }
            }
        }
    }

    if (!healthy)
        write_data(addr_base, data, size);

    return 1;
}

void write_data(uint16_t addr_base, uint8_t *data, uint8_t size) {
    uint8_t counter;
    uint8_t valid;
    int8_t newest = find_newest_sector(addr_base, size, &counter, &valid);
    uint8_t next_counter = newest < 0 ? 0 : (uint8_t)(counter + 1);

    // two copies in the next sectors after the newest one (skipping sectors that fail verification)
    uint8_t written[2];
    uint8_t copies = 0;
    for (uint8_t j = 0; j < SECTOR_NUMBER && copies < 2; ++j) {
        uint8_t idx = (uint8_t)((uint8_t)(newest + 1 + j) % SECTOR_NUMBER);     // newest >= -1
        if (write_sector_and_verify(SECTOR_ADDRES(addr_base, idx, size), next_counter, data, size)) {
            written[copies++] = idx;
            next_counter++;
        }
    }

    // invalidate all other (stale) sectors, only when both copies are in place
    if (copies == 2) {
        for (uint8_t i = 0; i < SECTOR_NUMBER; ++i) {
            uint16_t addr = SECTOR_ADDRES(addr_base, i, size);
            if (i != written[0] && i != written[1] && eeprom_read_byte(addr) == MAGIC_BYTE)
                eeprom_write_byte(addr, 0x00);
        }
    }
}
