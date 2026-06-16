# H9 Standard Node Registers

Every H9 node implements a set of standard registers (0–9) handled automatically
by the `can.c` library. Registers numbered 10 and above are application-specific
and passed to the application unchanged.

## Access model

Registers are accessed via unicast CAN messages directed to the node's ID:

| Operation | Message type       | Frame format                                    |
|-----------|--------------------|-------------------------------------------------|
| Read      | `GET_REG` (0x0A)   | `data[0]` = register number; `dlc` = 1          |
| Write     | `SET_REG` (0x09)   | `data[0]` = register number; `data[1..]` = value |

The node replies with `REG_VALUE` (0x0F): `data[0]` = register number, `data[1..]` = value.  
On error the node replies with `COMMAND_ERROR` (0x08): `data[0]` = error code.

## Standard registers

| # | Constant                         | Access | Size    | Description                     |
|---|----------------------------------|--------|---------|---------------------------------|
| 0 | `NODE_TYPE_STD_REGISTER`         | R      | 2 bytes | Node type                       |
| 1 | `NODE_HARDWARE_REVISION_STD_REGISTER` | R | 1 byte  | Hardware revision letter        |
| 2 | `NODE_VERSION_STD_REGISTER`      | R      | 4 bytes | Firmware version (major, minor) |
| 3 | `NODE_BUILD_INFO_STD_REGISTER`   | R      | ≤7 bytes| Build info string               |
| 4 | `NODE_MCU_TYPE_STD_REGISTER`     | R      | 1 byte  | MCU type enum                   |
| 5 | `NODE_SN_STD_REGISTER`           | R      | 4 bytes | Serial number                   |
| 6 | `NODE_RESET_REASON_STD_REGISTER` | R      | 1 byte  | Reason for last reset           |
| 7 | `NODE_POWER_SUPPLY_STD_REGISTER` | —      | —       | Not implemented                 |
| 8 | `NODE_MCU_TEMP_STD_REGISTER`     | —      | —       | Not implemented                 |
| 9 | `NODE_ID_STD_REGISTER`           | R/W    | 2 bytes | Node ID (9-bit)                 |

---

### Register 0 — NODE_TYPE

Read-only. Returns the 16-bit node type passed to `CAN_init()`.

```
GET_REG  data: [0x00]
REG_VALUE data: [0x00, type_hi, type_lo]
```

Known node types are listed in `doc/nodes.md`.

---

### Register 1 — NODE_HARDWARE_REVISION

Read-only. Returns a single ASCII character identifying the hardware revision
(`'a'`, `'b'`, …). Set at `CAN_init()`.

```
GET_REG   data: [0x01]
REG_VALUE data: [0x01, rev]       e.g. [0x01, 0x61] for 'a'
```

---

### Register 2 — NODE_VERSION

Read-only. Returns the firmware version as two 16-bit values (major, minor),
each in big-endian byte order.

```
GET_REG   data: [0x02]
REG_VALUE data: [0x02, major_hi, major_lo, minor_hi, minor_lo]
```

Example: version 1.3 → `[0x02, 0x00, 0x01, 0x00, 0x03]`

---

### Register 3 — NODE_BUILD_INFO

Read-only. Returns a build information string (e.g. git-describe output) as raw
bytes. The current implementation sends up to 7 bytes per frame (one CAN frame).
Full multi-frame transfer is not yet implemented (TODO in source).

```
GET_REG   data: [0x03]
REG_VALUE data: [0x03, b0, b1, b2, b3, b4, b5, b6]   (up to 7 bytes of string)
```

---

### Register 4 — NODE_MCU_TYPE

Read-only. Returns a 1-byte enum identifying the MCU. Determined at compile time
from the `__AVR_*__` predefined macro.

```
GET_REG   data: [0x04]
REG_VALUE data: [0x04, mcu_type]
```

| Value | Constant              | MCU            |
|-------|-----------------------|----------------|
| 1     | `NODE_MCU_ATMEGA16M1` | ATmega16M1     |
| 2     | `NODE_MCU_ATMEGA32M1` | ATmega32M1     |
| 3     | `NODE_MCU_ATMEGA64M1` | ATmega64M1     |
| 4     | `NODE_MCU_ATMEGA16C1` | ATmega16C1     |
| 5     | `NODE_MCU_ATMEGA32C1` | ATmega32C1     |
| 6     | `NODE_MCU_ATMEGA64C1` | ATmega64C1     |
| 7     | `NODE_MCU_AT90CAN128` | AT90CAN128     |
| 8     | `NODE_MCU_PIC18F46K80`| PIC18F46K80    |

---

### Register 5 — NODE_SN

Read-only. Intended for a unique hardware serial number. Currently always returns
four zero bytes.

```
GET_REG   data: [0x05]
REG_VALUE data: [0x05, 0x00, 0x00, 0x00, 0x00]
```

---

### Register 6 — NODE_RESET_REASON

Read-only. Returns the reason the node last reset. The value is captured before
the watchdog is disabled in the early `.init3` startup code, and preserved across
resets in a `.noinit` RAM variable.

```
GET_REG   data: [0x06]
REG_VALUE data: [0x06, reason]
```

| Value | Constant                       | Meaning                    |
|-------|--------------------------------|----------------------------|
| 0     | `NODE_RESET_BY_UNKNOW`         | Unknown / unclassified     |
| 1     | `NODE_RESET_BY_POWER_ON`       | Power-on (POR + BOR set)   |
| 2     | `NODE_RESET_BY_WATCHDOG`       | Watchdog timeout           |
| 3     | `NODE_RESET_BY_BROWN_OUT`      | Brown-out                  |
| 4     | `NODE_RESET_BY_EXTERNAL_SOURCE`| External reset pin         |

---

### Register 7 — NODE_POWER_SUPPLY

Not implemented. Returns `COMMAND_ERROR` / `H9FRAME_ERROR_INVALID_REGISTER`.
Reserved for supply voltage measurement.

---

### Register 8 — NODE_MCU_TEMP

Not implemented. Returns `COMMAND_ERROR` / `H9FRAME_ERROR_INVALID_REGISTER`.
Reserved for on-chip temperature sensor.

---

### Register 9 — NODE_ID

Read/write. The node's 9-bit CAN address (valid range 0–511).

**GET:**
```
GET_REG   data: [0x09]
REG_VALUE data: [0x09, id_hi, id_lo]
```
`id_hi` = bit 8 (0 or 1); `id_lo` = bits 7–0.

**SET:**
```
SET_REG   data: [0x09, id_hi, id_lo]    dlc = 3
REG_VALUE data: [0x09, id_hi, id_lo]   (echoes the value currently active)
```

The new ID is written to EEPROM immediately and takes effect after the next
reset. The response echoes the **current** (pre-reset) node ID, not the new one.

Wrong `dlc` (not 3) returns `COMMAND_ERROR` / `H9FRAME_ERROR_REGISTER_SIZE_MISMATCH`.

---

## Error codes

| Code | Constant                              | Meaning                                      |
|------|---------------------------------------|----------------------------------------------|
| 1    | `H9FRAME_ERROR_INVALID_MSG`           | Message type not valid for this node state   |
| 2    | `H9FRAME_ERROR_BOOTLOADER_UNSUPPORTED`| NODE_UPGRADE requested but no bootloader     |
| 3    | `H9FRAME_ERROR_UNSUPPORTED_OPERATION` | Operation not supported for this register    |
| 4    | `H9FRAME_ERROR_INVALID_REGISTER`      | Register number unknown                      |
| 5    | `H9FRAME_ERROR_READ_ONLY_REGISTER`    | Attempted write to a read-only register      |
| 6    | `H9FRAME_ERROR_WRITE_ONLY_REGISTER`   | Attempted read from a write-only register    |
| 7    | `H9FRAME_ERROR_REGISTER_SIZE_MISMATCH`| Wrong number of data bytes for this register |
