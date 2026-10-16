/*
 * h9atu
 *
 * Created by SQ8KFH on 2020-06-30.
 *
 * Copyright (C) 2020-2021 Kamil Palkowski. All rights reserved.
 */

#ifndef CONFIG_H
#define	CONFIG_H

#include <xc.h>

#if defined(_18FXXQ84_FAMILY_)
// PIC18F47Q84 Configuration Bit Settings
// Must be identical to the application config bits (both hex files are merged).

// CONFIG1
#pragma config FEXTOSC = HS     // External Oscillator Selection (HS (crystal oscillator) above 8 MHz)
#pragma config RSTOSC = EXTOSC  // Reset Oscillator Selection (EXTOSC operating per FEXTOSC bits)

// CONFIG2
#pragma config CLKOUTEN = OFF   // Clock out Enable bit (CLKOUT function is disabled)
#pragma config PR1WAY = ON      // PRLOCKED One-Way Set Enable bit
#pragma config CSWEN = ON       // Clock Switch Enable bit
#pragma config JTAGEN = OFF     // JTAG Enable bit (JTAG pins revert to user functions)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable bit
#pragma config FCMENP = OFF
#pragma config FCMENS = OFF

// CONFIG3
#pragma config MCLRE = EXTMCLR  // MCLR Enable bit
#pragma config PWRTS = PWRT_64  // Power-up timer selection bits (64 ms)
#pragma config MVECEN = OFF     // Multi-vector enable bit (legacy high/low priority vectors 0x08/0x18)
#pragma config IVT1WAY = ON
#pragma config LPBOREN = OFF
#pragma config BOREN = SBORDIS  // Brown-out Reset enabled, SBOREN bit is ignored

// CONFIG4
#pragma config BORV = VBOR_2P85 // Brown-out Reset Voltage (2.8V, highest available)
#pragma config ZCD = OFF
#pragma config PPS1WAY = OFF    // PPS registers can be changed repeatedly (bootloader and application)
#pragma config STVREN = ON      // Stack full/underflow will cause Reset
#pragma config LVP = ON         // Low voltage programming enabled (no PGM pin on this device)
#pragma config XINST = OFF

// CONFIG5
#pragma config WDTE = OFF       // WDT Disabled

// CONFIG7
#pragma config BBEN = OFF       // Boot block disabled
#pragma config SAFEN = OFF      // SAF disabled

// CONFIG8
#pragma config WRTB = OFF
#pragma config WRTC = OFF
#pragma config WRTD = OFF
#pragma config WRTSAF = OFF
#pragma config WRTAPP = OFF

// CONFIG10
#pragma config CP = OFF

#else
// PIC18F46K80 Configuration Bit Settings

// 'C' source line config statements

// CONFIG1L
#pragma config RETEN = OFF      // VREG Sleep Enable bit (Ultra low-power regulator is Disabled (Controlled by REGSLP bit))
#pragma config INTOSCSEL = HIGH // LF-INTOSC Low-power Enable bit (LF-INTOSC in High-power mode during Sleep)
#pragma config SOSCSEL = DIG    // SOSC Power Selection and mode Configuration bits (Digital (SCLKI) mode)
#pragma config XINST = OFF      // Extended Instruction Set (Disabled)

// CONFIG1H
#pragma config FOSC = HS1       // Oscillator (HS oscillator (Medium power, 4 MHz - 16 MHz))
#pragma config PLLCFG = OFF     // PLL x4 Enable bit (Disabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor (Disabled)
#pragma config IESO = OFF       // Internal External Oscillator Switch Over Mode (Disabled)

// CONFIG2L
#pragma config PWRTEN = ON      // Power Up Timer (Enabled)
#pragma config BOREN = SBORDIS  // Brown Out Detect (Enabled in hardware, SBOREN disabled)
#pragma config BORV = 0         // Brown-out Reset Voltage bits (3.0V)
#pragma config BORPWR = MEDIUM  // BORMV Power level (BORMV set to medium power level)

// CONFIG2H
#pragma config WDTEN = OFF      // Watchdog Timer (WDT disabled in hardware; SWDTEN bit disabled)
#pragma config WDTPS = 1048576  // Watchdog Postscaler (1:1048576)

// CONFIG3H
#pragma config CANMX = PORTB    // ECAN Mux bit (ECAN TX and RX pins are located on RB2 and RB3, respectively)
#pragma config MSSPMSK = MSK7   // MSSP address masking (7 Bit address masking mode)
#pragma config MCLRE = ON       // Master Clear Enable (MCLR Enabled, RE3 Disabled)

// CONFIG4L
#pragma config STVREN = ON      // Stack Overflow Reset (Enabled)
#pragma config BBSIZ = BB2K     // Boot Block Size (2K word Boot Block size)

// CONFIG5L
#pragma config CP0 = OFF        // Code Protect 00800-03FFF (Disabled)
#pragma config CP1 = OFF        // Code Protect 04000-07FFF (Disabled)
#pragma config CP2 = OFF        // Code Protect 08000-0BFFF (Disabled)
#pragma config CP3 = OFF        // Code Protect 0C000-0FFFF (Disabled)

// CONFIG5H
#pragma config CPB = OFF        // Code Protect Boot (Disabled)
#pragma config CPD = OFF        // Data EE Read Protect (Disabled)

// CONFIG6L
#pragma config WRT0 = OFF       // Table Write Protect 00800-03FFF (Disabled)
#pragma config WRT1 = OFF       // Table Write Protect 04000-07FFF (Disabled)
#pragma config WRT2 = OFF       // Table Write Protect 08000-0BFFF (Disabled)
#pragma config WRT3 = OFF       // Table Write Protect 0C000-0FFFF (Disabled)

// CONFIG6H
#pragma config WRTC = OFF       // Config. Write Protect (Disabled)
#pragma config WRTB = OFF       // Table Write Protect Boot (Disabled)
#pragma config WRTD = OFF       // Data EE Write Protect (Disabled)

// CONFIG7L
#pragma config EBTR0 = OFF      // Table Read Protect 00800-03FFF (Disabled)
#pragma config EBTR1 = OFF      // Table Read Protect 04000-07FFF (Disabled)
#pragma config EBTR2 = OFF      // Table Read Protect 08000-0BFFF (Disabled)
#pragma config EBTR3 = OFF      // Table Read Protect 0C000-0FFFF (Disabled)

// CONFIG7H
#pragma config EBTRB = OFF      // Table Read Protect Boot (Disabled)
#endif

#define _XTAL_FREQ 16000000


#endif	/* CONFIG_H */

