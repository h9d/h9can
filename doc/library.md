# h9can library

The library implements the node side of the [H9 protocol](protocol.md): CAN driver, receive and
transmit buffers, standard registers 0–9, `DISCOVER` / reset / upgrade handling, node ID storage
and node flags. The application only handles its own registers (≥ 10) and frames.

| | AVR | PIC |
|---|---|---|
| MCU | ATmega16M1, 32M1, 64M1, 32C1, AT90CAN128 | PIC18F46K80 |
| Sources | `avr/can.c` | `pic/can.c`, `pic/ee_mem.c` |
| Headers | `include/h9can.h` (API), `h9avr/can.h`, `h9avr/node_id.h`, `h9avr/bl_info.h` | `include/h9can.h` (API), `h9pic/can.h`, `h9pic/node_id.h`, `h9pic/ee_mem.h`, `h9pic/bl_info.h` |
| Example node | `power_switch` | `s-match` (v2) |

The public API is documented in [`include/h9can.h`](../include/h9can.h).

---

## Typical node

```c
#include <h9can.h>

int main(void) {
    // hardware init ...
    if (!CAN_init(NODE_TYPE, 0xff - NODE_TYPE, PCB_REVISION, BOM_REVISION,
                  VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, APP_VERSION)) {
        // no node ID stored in EEPROM, running with the default ID
    }
    // enable interrupts (AVR: sei())

    CAN_send_turned_on_broadcast();

    while (1) {
        h9frame_t cm;
        switch (CAN_get_msg(&cm)) {
            case 1:     // unicast for the application: registers >= 10, REG_VALUE / COMMAND_ERROR from other nodes
                if (cm.type == H9FRAME_TYPE_GET_REG && cm.data[0] == 10)
                    CAN_send_reg_value(10, cm.source_id, cm.unicast.seqnum, &status, 1);
                else
                    send_command_error(H9FRAME_ERROR_INVALID_REGISTER, cm.source_id, cm.unicast.seqnum);
                break;
            case 2:     // broadcast received through the filters (other nodes' NODE_INFO, REG_VALUE_BROADCAST, ...)
                break;
            default:    // nothing, or handled by the library
                break;
        }
    }
}
```

- `NODE_TYPE`, `PCB_REVISION`, `BOM_REVISION` come from the project's `CMakeLists.txt`; the same
  values are compiled into the bootloader ([`bootloader.md`](bootloader.md)).
- `CAN_get_msg()` must be called often: the library answers standard registers, `DISCOVER` and
  resets from it, and samples the CAN error state for the node flags.
- Every request returned to the application must be answered (`CAN_send_reg_value()` or
  `send_command_error()`) with the request's `seqnum`, see
  [`standard_registers.md`](standard_registers.md#application-registers--10).

### Sending frames

```c
h9frame_t f;
f.type = H9FRAME_TYPE_REG_VALUE_BROADCAST;      // broadcast: the group is set to the node type
f.dlc = 2;
f.data[0] = 10;
f.data[1] = status;
CAN_put_msg(&f);
```

`CAN_put_msg()` fills `source_id` (node ID) and, for broadcasts, the group (node type). For a
unicast frame set `unicast.destination_id`, `unicast.seqnum` and `unicast.flags` (0).

### Application hooks

| Purpose | AVR | PIC |
|---|---|---|
| Register 7 (supply voltage) | define `void read_power_supply_register(uint8_t dst, uint8_t seqnum)` (weak default) | assign the pointer `read_power_supply_register` |
| Register 8 (MCU temperature) | define `void read_mcu_temp_register(uint8_t dst, uint8_t seqnum)` (weak default) | — |
| CAN interrupt | built into the library (`CAN_INT_vect` / `CANIT_vect`) | call `can_interrupt()` from the low-priority ISR |

Example (s-match, PIC):

```c
void __interrupt(low_priority) low_isr(void) {
    can_interrupt();
}
```

---

## AVR

### Project

A new node project is created from `template/` (see the repository `README.md`). The project
sets `AVR_MCU`, `AVR_F_CPU` (4, 12 or 16 MHz) and the node identity, and includes
`cmake/avr.cmake`, which:

- builds the library for the MCU / clock (`h9can_<mcu>_<freq>`, with `BOOTSTART` for `NODE_UPGRADE`),
- passes `NODE_TYPE`, `PCB_REVISION`, `BOM_REVISION` to the application,
- builds the bootloader with the same identity,
- with `cmake/avrdude-helpers.cmake`: `flash` (application only, chip erase — removes the
  bootloader), `flash-all` (bootloader + application), `fuse`, `read-fuses`, `verify`.

### Resources

| Resource | Use |
|---|---|
| MOb 0 | TX |
| MOb 1 | RX unicast: types 8–15 addressed to the node ID |
| MOb 2 | RX `DISCOVER` / `GROUP_RESET` (types 16–17) to group 0xFFFF |
| MOb 3 | RX `DISCOVER` / `GROUP_RESET` to the node's own type |
| MOb 4, 5 | `CAN_set_msg_filter_1()` / `_2()`: broadcasts (types 16–31) of a given group |
| TX queue | 8 frames (MOb 0 busy → queued, sent from the TX interrupt) |
| RX buffer | 16 frames (filled in the CAN interrupt, read by `CAN_get_msg()`) |
| EEPROM | node ID 0x10–0x5F and 0x80–0xCF (`h9avr/node_id.h`), 0x00–0x0F unused |
| Watchdog | `NODE_RESET` / `GROUP_RESET` (15 ms watchdog reset) |
| `.init3` | reset reason captured from `MCUSR`, watchdog disabled before `main()` |

Broadcasts of other nodes are received only through the optional filters.

### Transmit errors

`CAN_put_msg()` returns 1 (sent), 2 (queued) or 0 (dropped: queue full). When the queue is full
and the controller is error passive / bus off, the newest queued frame is replaced by
`NODE_FAULT` / `CAN_FRAME_LOSS` and `NODE_FLAG_CAN_TX_FRAME_LOSS` is set.

---

## PIC

### Project

The PIC node (MPLAB X / XC8) compiles `pic/can.c` and `pic/ee_mem.c` with `include/` on the
include path. The application:

- calls `can_interrupt()` from the low-priority interrupt,
- may assign `read_power_supply_register`,
- may store its own data with the EEPROM sector API (`write_data()` / `read_data_and_refresh()`
  at `USER_BASE`, two copies with counters and CRC, see `h9pic/ee_mem.h`).

The clock is fixed to 16 MHz (CAN bit timing in `CAN_init()`).

### Resources (ECAN legacy mode 0)

| Resource | Use |
|---|---|
| RXB0: RXF0, RXF1 + RXM0 | unicast: types 8–15 addressed to the node ID |
| RXB1: RXF2 + RXM1 | broadcasts (types 16–31) to group 0xFFFF |
| RXB1: RXF3 | broadcasts of the node's own type |
| RXB1: RXF4, RXF5 | `CAN_set_msg_filter_1()` / `_2()`: broadcasts of a given group (written in configuration mode) |
| TXB0–TXB2 | TX (`CAN_put_msg()` waits for a free buffer with interrupts enabled) |
| RX buffer | 16 frames (filled in `can_interrupt()`) |
| EEPROM | node ID 0x100–0x17F (`h9pic/node_id.h`), application data from `USER_BASE` (0x80) |
| User ID 0x200001–4 | serial number (register 6, see [`SN.md`](SN.md)) |

Unlike the AVR filters, RXF2 / RXF3 accept all broadcast types of their group, so the
application also gets e.g. `NODE_INFO` of nodes of the same type (`CAN_get_msg()` returns 2).

### Transmit errors

`CAN_put_msg()` returns 1 (loaded into a TX buffer) or 0. When all three buffers are busy and the
controller is error passive / bus off, TXB2 is replaced by `NODE_FAULT` / `CAN_FRAME_LOSS`.
