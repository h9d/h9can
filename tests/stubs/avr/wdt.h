#pragma once

#define WDTO_15MS 0

static inline void wdt_enable(int timeout) { (void)timeout; }
static inline void wdt_disable(void) {}
