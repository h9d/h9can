#pragma once

#define cli() ((void)0)
#define sei() ((void)0)

/* ISR block in can.c is guarded by #ifdef __AVR__, so this macro is never used.
   Defined here to avoid a compile error if included without the guard. */
#define ISR(vector) void __avr_isr_##vector(void)
