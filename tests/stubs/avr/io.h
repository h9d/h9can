#pragma once
#include <stdint.h>

/* CAN controller registers — each TU gets its own static copy (harmless for tests) */
static volatile uint8_t CANGCON, CANGSTA, CANGIT, CANHPMOB;
static volatile uint8_t CANTCON;
static volatile uint8_t CANBT1, CANBT2, CANBT3;
static volatile uint8_t CANPAGE;
static volatile uint8_t CANSTMOB, CANCDMOB, CANMSG;
static volatile uint8_t CANIDT1, CANIDT2, CANIDT3, CANIDT4;
static volatile uint8_t CANIDM1, CANIDM2, CANIDM3, CANIDM4;
static volatile uint8_t CANEN1, CANEN2;
static volatile uint8_t CANIE1, CANIE2;
static volatile uint8_t CANGIE;
static volatile uint8_t MCUSR;

/* CANGCON bits */
#define SWRES   0
#define ENASTB  1

/* CANPAGE bits */
#define MOBNB0  4

/* CANCDMOB bits */
#define CONMOB0  6
#define CONMOB1  7
#define IDE      4
#define RPLV     5

/* CANSTMOB bits */
#define RXOK  5
#define TXOK  6

/* CANEN2 bits */
#define ENMOB0  0
#define ENMOB1  1
#define ENMOB2  2
#define ENMOB3  3
#define ENMOB4  4
#define ENMOB5  5

/* CANIE2 bits */
#define IEMOB0  0
#define IEMOB1  1
#define IEMOB2  2
#define IEMOB3  3
#define IEMOB4  4
#define IEMOB5  5

/* CANGIE bits */
#define ENERG   0
#define ENBX    1
#define ENERR   2
#define ENTX    3
#define ENRX    4
#define ENIT    5
#define ENBOFF  6

/* CANIDM4 bits */
#define IDEMSK  0
#define RTRMSK  2

/* MCU reset source bits (used in wdt_init, which is guarded by #ifdef __AVR__) */
#define PORF   0
#define BORF   2
#define WDRF   3
#define EXTRF  1
