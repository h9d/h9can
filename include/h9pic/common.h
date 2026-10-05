#ifndef COMMON_H
#define COMMON_H

//          | SIDH                        | SIDL                    | EIDH                    | EIDL
// 31 30 29 | 28     27 26 25 24 23 22 21 | 20 19 18 ** ** ** 17 16 | 15 14 13 12 11 10 09 08 | 07 06 05 04 03 02 01 00
// -- -- -- | ty_(0) ty ty ty ty so so so | so so so **  1 ** so so | fl fl fl ds ds ds ds ds | ds ds ds sq sq sq sq sq
static void calc_can_unicast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t src, uint8_t flags, uint8_t dst, uint8_t seq) {
    *id1 = (uint8_t)(type << 3) | (uint8_t)(src >> 5);
    *id2 = (uint8_t)((src << 3) & 0xe0) | 0x08 | (src & 0x03);
    *id3 = (uint8_t)(flags << 5) | (uint8_t)(dst >> 3);
    *id4 = (uint8_t)(dst << 5) | (seq & 0x1f);
}

//          | SIDH                        | SIDL                    | EIDH                    | EIDL
// 31 30 29 | 28     27 26 25 24 23 22 21 | 20 19 18 ** ** ** 17 16 | 15 14 13 12 11 10 09 08 | 07 06 05 04 03 02 01 00
// -- -- -- | ty_(1) ty ty ty ty so so so | so so so **  1 ** so so | nt nt nt nt nt nt nt nt | nt nt nt nt nt nt nt nt
static void calc_can_broadcast_id(volatile uint8_t *id1, volatile uint8_t *id2, volatile uint8_t *id3, volatile uint8_t *id4, uint8_t type, uint8_t src, uint16_t node_type) {
    *id1 = (uint8_t)(type << 3) | (uint8_t)(src >> 5);
    *id2 = (uint8_t)((src << 3) & 0xe0) | 0x08 | (src & 0x03);
    *id3 = node_type >> 8;
    *id4 = node_type & 0xff;
}

#endif
