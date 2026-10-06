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
| 0 | `NODE_FLAGS_STD_REGISTER`        | R      | 2 bytes | Node flags (reset reason, bootloader, CAN state) |
| 1 | `NODE_TYPE_STD_REGISTER`         | R      | 2 bytes | Node type                       |
| 2 | `NODE_HARDWARE_REVISION_STD_REGISTER` | R | 2 bytes | PCB revision letter, BOM revision |
| 3 | `NODE_VERSION_STD_REGISTER`      | R      | 6 bytes | Firmware version (major, minor, patch) |
| 4 | `NODE_BUILD_INFO_STD_REGISTER`   | R      | ≤31 bytes | Build info string             |
| 5 | `NODE_MCU_TYPE_STD_REGISTER`     | R      | 1 byte  | MCU type enum                   |
| 6 | `NODE_SN_STD_REGISTER`           | R      | 10 bytes (AVR) / 4 bytes (PIC) | Serial number |
| 7 | `NODE_POWER_SUPPLY_STD_REGISTER` | —      | —       | Not implemented                 |
| 8 | `NODE_MCU_TEMP_STD_REGISTER`     | —      | —       | Not implemented                 |
| 9 | `NODE_ID_STD_REGISTER`           | R/W    | 2 bytes | Node ID (9-bit)                 |

---

### Register 0 — NODE_FLAGS

Read-only. 16-bit node flags, big-endian. The same value is sent in `data[6..7]`
of `NODE_INFO` and `NODE_TURNED_ON`.

```
GET_REG   data: [0x00]
REG_VALUE data: [0x00, flags_hi, flags_lo]
```

| Bits | Constant                       | Meaning |
|------|--------------------------------|---------|
| 0–2  | `NODE_FLAG_RESET_REASON_MASK`  | Reason for the last reset, `NODE_RESET_BY_*` (see below) |
| 3    | `NODE_FLAG_BL_PRESENT`         | Bootloader present |
| 4    | `NODE_FLAG_BL_MISMATCH`        | Bootloader built for another node type / PCB / BOM revision |
| 5    | `NODE_FLAG_DEFAULT_ID`         | No node ID in EEPROM, the default ID passed to `CAN_init()` is used |
| 6    | `NODE_FLAG_CAN_ERROR_WARNING`  | CAN error warning / error passive / bus off occurred since start |
| 7    | `NODE_FLAG_CAN_TX_FRAME_LOSS`  | A frame could not be sent since start (TX queue full while bus passive / bus off) |
| 8    | `NODE_FLAG_CAN_RX_FRAME_LOSS`  | A received frame was dropped since start (receive buffer overflow) |
| 9–15 | —                              | Reserved (0) |

Bits 0–5 are set at start in `CAN_init()`; bits 6–8 are sticky and set at runtime.

Bootloader detection: the bootloader info block at the end of flash
(`h9avr/bl_info.h`, `h9pic/bl_info.h` — same 10-byte layout): bit 3 is set when the
block is valid, bit 4 when its node type / PCB / BOM revision differ from the values
passed to `CAN_init()`. A bootloader without the block (older version) is reported
as not present.
- **AVR** — block at `FLASHEND + 1 - 10`; not readable when the boot section is read locked (BLB1x).
- **PIC** — block at `_ROMSIZE - 10` (PIC18F46K80: 0xFFF6, inside the bootloader area
  0xF600-0xFFFF); not readable when table reads of that block are protected (EBTR3).

Reset reason (bits 0–2):

| Value | Constant                        | Meaning                    |
|-------|---------------------------------|----------------------------|
| 0     | `NODE_RESET_BY_UNKNOWN`         | Unknown / unclassified     |
| 1     | `NODE_RESET_BY_POWER_ON`        | Power-on                   |
| 2     | `NODE_RESET_BY_WATCHDOG`        | Watchdog timeout (AVR: also `NODE_RESET` over CAN) |
| 3     | `NODE_RESET_BY_BROWN_OUT`       | Brown-out                  |
| 4     | `NODE_RESET_BY_EXTERNAL_SOURCE` | External reset pin (AVR)   |
| 5     | `NODE_RESET_BY_SOFTWARE`        | RESET instruction (PIC, also `NODE_RESET` over CAN) |

---

### Register 1 — NODE_TYPE

Read-only. Returns the 16-bit node type passed to `CAN_init()`.

```
GET_REG  data: [0x01]
REG_VALUE data: [0x01, type_hi, type_lo]
```

Known node types are listed in `doc/nodes.md`.

---

### Register 2 — NODE_HARDWARE_REVISION

Read-only. Returns the PCB revision as an ASCII letter (`'A'`, `'B'`, …)
followed by the BOM revision (uint8). Both are passed to `CAN_init()`.

```
GET_REG   data: [0x02]
REG_VALUE data: [0x02, pcb, bom]  e.g. [0x02, 0x42, 0x01] for PCB 'B', BOM 1
```

---

### Register 3 — NODE_VERSION

Read-only. Returns the firmware version as three 16-bit values (major, minor, patch),
each in big-endian byte order.

```
GET_REG   data: [0x03]
REG_VALUE data: [0x03, major_hi, major_lo, minor_hi, minor_lo, patch_hi, patch_lo]
```

Example: version 1.3.2 → `[0x03, 0x00, 0x01, 0x00, 0x03, 0x00, 0x02]`

---

### Register 4 — NODE_BUILD_INFO

Read-only. Returns the build information string (e.g. git-describe output) as raw
bytes, up to 31 bytes; longer than 7 bytes is sent as a multi-frame `REG_VALUE`.

```
GET_REG   data: [0x04]
REG_VALUE data: [0x04, b0, b1, ...]
```

---

### Register 5 — NODE_MCU_TYPE

Read-only. Returns a 1-byte enum identifying the MCU, determined at compile time.

```
GET_REG   data: [0x05]
REG_VALUE data: [0x05, mcu_type]
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

### Register 6 — NODE_SN

Read-only. Unique hardware serial number; the size depends on the MCU:

- **AVR (ATmega16/32/64 M1/C1)** — 10 bytes, the factory serial number from the
  signature row (addresses 0x000E–0x0017: lot number, wafer number and die X/Y
  coordinates). Not documented for the M1/C1 family, the same block is documented
  as the serial number for ATmega328PB. Sent as a multi-frame `REG_VALUE`.
  Not available on AT90CAN128 (returns `H9FRAME_ERROR_UNSUPPORTED_REGISTER`).
- **PIC (PIC18F46K80)** — 4 bytes from User ID memory (0x200001–0x200004), written
  when programming the device (see `doc/SN.md`).

```
GET_REG   data: [0x06]
REG_VALUE data: [0x06, sn0, sn1, ...]
```

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

Read/write. The node's 8-bit CAN address, valid range 1–254.

**GET:**
```
GET_REG   data: [0x09]
REG_VALUE data: [0x09, id]
```

**SET:**
```
SET_REG   data: [0x09, id]    dlc = 2
REG_VALUE data: [0x09, id]    (echoes the id currently active)
```

The new ID is written to EEPROM immediately and takes effect after the next
reset. The response echoes the **current** (pre-reset) node ID, not the new one.

- Wrong `dlc` (not 2) returns `COMMAND_ERROR` / `H9FRAME_ERROR_REGISTER_SIZE_MISMATCH`.
- ID 0 or 0xFF returns `COMMAND_ERROR` / `H9FRAME_ERROR_INVALID_VALUE`
  (0 means "no ID stored", the node then starts with the default ID).

EEPROM storage (AVR, `h9avr/node_id.h`): two independent copies, each a ring of
10 blocks (0x10–0x5F and 0x80–0xCF; address 0 is left unused, it is the most exposed
to corruption on brown-out). Each 8-byte block (two whole EEPROM pages) holds a
sequence number and a CRC; the valid block with the newest sequence number from
either copy is used, so an interrupted write or a damaged EEPROM page never loses
the ID. `CAN_init()` restores a damaged copy from the good one (EEPROM is written
only when a copy needs repair).

EEPROM storage (PIC, `pic/ee_mem.c`): 16 interleaved sectors at 0x100–0x17F (0x00–0x7F
is left unused). Every write stores the ID in two sectors (consecutive counters) and
invalidates all other sectors; the valid sector with the newest counter is used.
`CAN_init()` (`read_node_id_and_refresh()`) rewrites both copies only if one is missing
or damaged, or stale sectors are left. The same mechanism stores application data
at `USER_BASE` (0x80).

---

## Error codes

| Code | Constant                              | Meaning                                      |
|------|---------------------------------------|----------------------------------------------|
| 1    | `H9FRAME_ERROR_INVALID_FRAME`         | Invalid frame / message type not valid here  |
| 2    | `H9FRAME_ERROR_BOOTLOADER_UNSUPPORTED`| NODE_UPGRADE requested but no bootloader     |
| 3    | `H9FRAME_ERROR_UNSUPPORTED_OPERATION` | Operation not supported for this register    |
| 4    | `H9FRAME_ERROR_UNSUPPORTED_REGISTER`  | Register not supported by this node          |
| 5    | `H9FRAME_ERROR_INVALID_REGISTER`      | Register number unknown                      |
| 6    | `H9FRAME_ERROR_READ_ONLY_REGISTER`    | Attempted write to a read-only register      |
| 7    | `H9FRAME_ERROR_WRITE_ONLY_REGISTER`   | Attempted read from a write-only register    |
| 8    | `H9FRAME_ERROR_REGISTER_SIZE_MISMATCH`| Wrong number of data bytes for this register |
| 9    | `H9FRAME_ERROR_INVALID_VALUE`         | Value out of the allowed range               |
