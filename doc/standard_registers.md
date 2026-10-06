# H9 Standard Node Registers

Every H9 node implements the standard registers 0–9; they are handled by the h9can library
inside `CAN_get_msg()`. Registers 10 and above belong to the application (see
[Application registers](#application-registers--10)).

Frame formats are described in [`protocol.md`](protocol.md).

---

## Access

Registers are accessed with unicast frames sent to the node ID:

| Operation   | Request (type)        | Request payload        | Response |
|-------------|-----------------------|------------------------|----------|
| Read        | `GET_REG` (11)        | `[reg]`, dlc = 1        | `REG_VALUE` (9): `[reg, value...]` |
| Write       | `SET_REG` (10)        | `[reg, value...]`       | `REG_VALUE`: `[reg, value after the write]` |
| Set bit     | `SET_BIT` (12)        | `[reg, bit]`            | `REG_VALUE`: `[reg, value after the change]` |
| Clear bit   | `CLEAR_BIT` (13)      | `[reg, bit]`            | `REG_VALUE`: `[reg, value after the change]` |

- Values are big-endian; values longer than 7 bytes are sent as a multi-frame `REG_VALUE`
  ([`protocol.md`](protocol.md#multi-frame-transfers)).
- The response copies the request's `seqnum`.
- On error the node answers `COMMAND_ERROR` (8) with `[error]`
  ([error codes](protocol.md#error-codes)).

Standard registers (0–9) follow these rules:

| Request | Result |
|---------|--------|
| `GET_REG` with dlc = 1 | value of the register |
| `SET_REG` with dlc > 1 on a read-only register | `READ_ONLY_REGISTER` |
| `SET_BIT` / `CLEAR_BIT`, `GET_REG` with dlc ≠ 1, `SET_REG` with dlc = 1 | `UNSUPPORTED_OPERATION` |
| register not implemented on this node | `UNSUPPORTED_REGISTER` |

---

## Register list

| #  | Constant (`*_STD_REGISTER`) | Access | Size | Content |
|---:|-----------------------------|:------:|------|---------|
| 0  | `NODE_FLAGS`                | R      | 2 B  | Node flags (reset reason, bootloader, CAN state) |
| 1  | `NODE_TYPE`                 | R      | 2 B  | Node type |
| 2  | `NODE_HARDWARE_REVISION`    | R      | 2 B  | PCB revision (letter), BOM revision |
| 3  | `NODE_VERSION`              | R      | 6 B  | Firmware version major, minor, patch |
| 4  | `NODE_BUILD_INFO`           | R      | ≤ 31 B | Build info string |
| 5  | `NODE_MCU_TYPE`             | R      | 1 B  | MCU type |
| 6  | `NODE_SN`                   | R      | 10 B (AVR) / 4 B (PIC) | Serial number |
| 7  | `NODE_POWER_SUPPLY`         | R      | 4 B  | Supply voltage — provided by the application |
| 8  | `NODE_MCU_TEMP`             | R      | 4 B  | MCU temperature — provided by the application (AVR) |
| 9  | `NODE_ID`                   | R/W    | 1 B  | Node ID |

---

### Register 0 — NODE_FLAGS

16-bit node flags, the same value as `data[6..7]` of `NODE_INFO` / `NODE_TURNED_ON`.

```
GET_REG   [0x00]
REG_VALUE [0x00, flags_hi, flags_lo]
```

| Bits | Constant                       | Meaning |
|------|--------------------------------|---------|
| 0–2  | `NODE_FLAG_RESET_REASON_MASK`  | Reason for the last reset (`NODE_RESET_BY_*`) |
| 3    | `NODE_FLAG_BL_PRESENT`         | Bootloader present |
| 4    | `NODE_FLAG_BL_MISMATCH`        | Bootloader built for another node type / PCB / BOM revision |
| 5    | `NODE_FLAG_DEFAULT_ID`         | No node ID in EEPROM, the default ID is used |
| 6    | `NODE_FLAG_CAN_ERROR_WARNING`  | CAN error warning / error passive / bus off occurred since start |
| 7    | `NODE_FLAG_CAN_TX_FRAME_LOSS`  | A frame could not be sent since start |
| 8    | `NODE_FLAG_CAN_RX_FRAME_LOSS`  | A received frame was dropped since start |
| 9–15 | —                              | Reserved (0) |

Bits 0–5 are set at start, bits 6–8 are sticky. Reset reason codes and details:
[`protocol.md`](protocol.md#node-flags).

---

### Register 1 — NODE_TYPE

Node type passed to `CAN_init()` (see [`nodes.md`](nodes.md)); also the broadcast group of
the node's frames.

```
GET_REG   [0x01]
REG_VALUE [0x01, type_hi, type_lo]
```

---

### Register 2 — NODE_HARDWARE_REVISION

PCB revision as an ASCII letter (`'A'`, `'B'`, …) and BOM revision, both passed to `CAN_init()`.

```
GET_REG   [0x02]
REG_VALUE [0x02, pcb, bom]        e.g. [0x02, 0x42, 0x01] = PCB 'B', BOM 1
```

---

### Register 3 — NODE_VERSION

Firmware version passed to `CAN_init()`, three 16-bit values.

```
GET_REG   [0x03]
REG_VALUE [0x03, major_hi, major_lo, minor_hi, minor_lo, patch_hi, patch_lo]
```

Example: 1.3.2 → `[0x03, 0x00, 0x01, 0x00, 0x03, 0x00, 0x02]`.
(`NODE_INFO` carries the same version packed into 32 bits.)

---

### Register 4 — NODE_BUILD_INFO

Build info string passed to `CAN_init()` (e.g. `git describe` output), without the
terminating zero, up to 31 bytes; longer than 7 bytes is sent as a multi-frame `REG_VALUE`.

```
GET_REG   [0x04]
REG_VALUE [0x04, c0, c1, ...]
```

---

### Register 5 — NODE_MCU_TYPE

MCU type, fixed at compile time.

```
GET_REG   [0x05]
REG_VALUE [0x05, mcu]
```

| Value | Constant (`NODE_MCU_*`) | MCU |
|------:|-------------------------|-----|
| 1 | `ATMEGA16M1`  | ATmega16M1 |
| 2 | `ATMEGA32M1`  | ATmega32M1 |
| 3 | `ATMEGA64M1`  | ATmega64M1 |
| 4 | `ATMEGA16C1`  | ATmega16C1 |
| 5 | `ATMEGA32C1`  | ATmega32C1 |
| 6 | `ATMEGA64C1`  | ATmega64C1 |
| 7 | `AT90CAN128`  | AT90CAN128 |
| 8 | `PIC18F46K80` | PIC18F46K80 |

The AVR library is built for ATmega16M1, 32M1, 64M1, 32C1 and AT90CAN128.

---

### Register 6 — NODE_SN

Hardware serial number; the size depends on the MCU:

- **AVR (ATmega16/32/64 M1/C1)** — 10 bytes, the factory serial number from the signature row
  (addresses 0x000E–0x0017: lot number, wafer number, die X/Y coordinates). The block is not
  documented for the M1/C1 family; the same block is documented as the serial number for
  ATmega328PB. Not available on AT90CAN128 (`UNSUPPORTED_REGISTER`). Multi-frame response.
- **PIC (PIC18F46K80)** — 4 bytes from User ID memory 0x200001–0x200004, written when the
  device is programmed (see [`SN.md`](SN.md)).

```
GET_REG   [0x06]
REG_VALUE [0x06, sn0, sn1, ...]
```

---

### Register 7 — NODE_POWER_SUPPLY

Supply voltage, measured by the application (the library has no ADC code). By convention a
32-bit value in **millivolts**:

```
GET_REG   [0x07]
REG_VALUE [0x07, mv_3, mv_2, mv_1, mv_0]
```

The application provides the value:

- **AVR** — define `void read_power_supply_register(uint8_t destination_id, uint8_t seqnum)`;
  it overrides the library's weak default (which answers `UNSUPPORTED_REGISTER`) and sends the
  value with `CAN_send_reg_value()`.
- **PIC** — assign the function to the pointer `read_power_supply_register` (`h9pic/can.h`);
  `NULL` (default) answers `UNSUPPORTED_REGISTER`.

---

### Register 8 — NODE_MCU_TEMP

MCU temperature, measured by the application. By convention a signed 32-bit value in **°C**.

- **AVR** — define `void read_mcu_temp_register(uint8_t destination_id, uint8_t seqnum)`
  (weak default answers `UNSUPPORTED_REGISTER`). The ATmega16/32/64M1 temperature sensor
  calibration (TSOFFSET 0x0005, TSGAIN 0x0007, silicon revision 0x003F in the signature row)
  is described in the automotive datasheet 7647, section 18.8.2.
- **PIC** — not supported (`UNSUPPORTED_REGISTER`).

```
GET_REG   [0x08]
REG_VALUE [0x08, t_3, t_2, t_1, t_0]
```

---

### Register 9 — NODE_ID

The node ID (1–254).

```
GET_REG   [0x09]
REG_VALUE [0x09, id]

SET_REG   [0x09, id]      dlc = 2
REG_VALUE [0x09, id]      the ID currently in use, not the new one
```

- The new ID is written to EEPROM at once and used after the next reset.
- dlc ≠ 2 → `REGISTER_SIZE_MISMATCH`.
- AVR: ID 0 or 0xFF → `INVALID_VALUE` (0 means "no ID"). The PIC library does not check the value.

EEPROM storage:

- **AVR** (`h9avr/node_id.h`) — two independent copies, each a ring of 10 blocks
  (0x10–0x5F and 0x80–0xCF; address 0 is left unused, it is the most exposed to corruption on
  brown-out). Each 8-byte block (two whole EEPROM pages) holds a sequence number and a CRC; the
  valid block with the newest sequence number from either copy is used, so an interrupted write
  or a damaged page never loses the ID. `CAN_init()` repairs a damaged copy (writes only when
  needed).
- **PIC** (`h9pic/node_id.h`, `pic/ee_mem.c`) — 16 interleaved sectors at 0x100–0x17F
  (0x00–0x7F unused). Every write stores the ID in two sectors and invalidates the others; the
  valid sector with the newest counter is used. `CAN_init()` rewrites the copies only if one is
  missing or damaged. The same mechanism stores application data at `USER_BASE` (0x80).

---

## Application registers (≥ 10)

`GET_REG`, `SET_REG`, `SET_BIT` and `CLEAR_BIT` for registers 10 and above are returned by
`CAN_get_msg()` (return value 1). The application must answer every such request with the
request's `seqnum`:

- with the value — `CAN_send_reg_value(reg, cm.source_id, cm.unicast.seqnum, value, size)`;
  for `SET_REG` / `SET_BIT` / `CLEAR_BIT` the value after the change;
- or with an error — `send_command_error(error, cm.source_id, cm.unicast.seqnum)`, by
  convention `INVALID_REGISTER` for an unknown register, `READ_ONLY_REGISTER`,
  `UNSUPPORTED_OPERATION` (e.g. `SET_BIT` on a register that is not a bit field),
  `REGISTER_SIZE_MISMATCH`.

Register layouts of the node types are described in the h9d node description files
(`conf/nodes/*.conf` in h9d).
