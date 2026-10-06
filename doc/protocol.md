# H9 CAN Protocol

Wire-level description of the H9 bus: CAN identifiers, frame types and their payloads,
multi-frame transfers, node life cycle, node flags and error codes.

Related documents:
- [`standard_registers.md`](standard_registers.md) — standard registers 0–9 handled by every node,
- [`bootloader.md`](bootloader.md) — firmware upgrade protocol and bootloaders,
- [`library.md`](library.md) — using the h9can library in a node (AVR / PIC),
- [`nodes.md`](nodes.md) — node types.

All constants are defined in `include/h9def.h` (frame types, flags, errors, registers)
and `include/h9frame.h` (`h9frame_t`, filter groups).

---

## CAN layer

- CAN 2.0B, **29-bit extended identifiers only** (standard frames are ignored),
- **125 kbit/s**, sample point 75% (AVR: `CANBTx` for 4 / 12 / 16 MHz, PIC: `BRGCONx` for 16 MHz),
- up to 8 data bytes per frame; all multi-byte values in the payload are **big-endian**.

### Identifier layout

Bit 4 of `type` selects the layout.

**Unicast** (`type` 0–15):

| Bits | 28–24 | 23–16       | 15–13  | 12–5             | 4–0      |
|------|:-----:|:-----------:|:------:|:----------------:|:--------:|
|      | type  | source_id   | flags  | destination_id   | seqnum   |
| Size | 5     | 8           | 3      | 8                | 5        |

**Broadcast** (`type` 16–31):

| Bits | 28–24 | 23–16       | 15–0              |
|------|:-----:|:-----------:|:-----------------:|
|      | type  | source_id   | broadcast group   |
| Size | 5     | 8           | 16                |

| Field         | Description |
|---------------|-------------|
| `type`        | Frame type (`H9FRAME_TYPE_*`), see below. Bit 4 = 1 → broadcast. |
| `source_id`   | Node ID of the sender. The host (h9d) uses its own ID, configured in h9d. |
| `flags`       | Multi-frame marker (`H9FRAME_FLAG_*`), unicast only, see [Multi-frame transfers](#multi-frame-transfers). |
| `destination_id` | Node ID of the recipient. |
| `seqnum`      | Request / response correlation number, see [Sequence numbers](#sequence-numbers). |
| `broadcast group` | Node type group: frames sent **by** a node carry the sender's node type; frames sent **to** nodes (`DISCOVER`, `GROUP_RESET`) carry the target node type. |

Special broadcast groups:

| Group    | Constant                             | Meaning |
|----------|--------------------------------------|---------|
| `0xFFFF` | `H9FRAME_BROADCAST_ALL_GROUP`        | All node types (`DISCOVER` / `GROUP_RESET` to every node) |
| `0xFFFE` | `H9FRAME_BROADCAST_BOOTLOADER_GROUP` | Reserved (formerly the bootloader group; bootloaders now send with their node type) |

### Type groups

The upper type bits group the frames; the hardware filters of the nodes use these groups:

| Types  | Group               | Group / mask (`H9FRAME_*_MSG_TYPE_GROUP[_MASK]`) | Content |
|--------|---------------------|-------------|---------|
| 0–7    | bootloader          | `0` / `24`  | firmware upgrade, only accepted by a bootloader |
| 8–15   | unicast             | `8` / `24`  | registers, errors, reset, upgrade |
| 16–17  | special broadcast   | `16` / `30` | `DISCOVER`, `GROUP_RESET` (addressed **to** a group) |
| 16–31  | all broadcasts      | `16` / `16` | everything broadcast |

---

## Node addressing

- Node IDs are 8-bit, valid range **1–254**. 0 means "no ID", 0xFF is reserved.
- The ID is stored in EEPROM and read by `CAN_init()`. A node without a stored ID starts with
  the `default_id` passed to `CAN_init()` (the node projects use `0xFF - NODE_TYPE`) and sets
  `NODE_FLAG_DEFAULT_ID`.
- The ID is changed with register 9 (`NODE_ID`); the new ID is used after the next reset.
- A bootloader reads the same EEPROM; without a stored ID it uses ID 0.

---

## Frame types

| Type | Constant (`H9FRAME_TYPE_*`) | Kind      | Sender → receiver | Response |
|-----:|-----------------------------|-----------|-------------------|----------|
| 0    | `RES1`                      | unicast   | — (reserved)      | — |
| 1    | `PAGE_START`                | unicast   | host → bootloader | `PAGE_FILL_NEXT` |
| 2    | `QUIT_BOOTLOADER`           | unicast   | host → bootloader | (application starts: `NODE_TURNED_ON`) |
| 3    | `PAGE_FILL`                 | unicast   | host → bootloader | `PAGE_FILL_NEXT` / `PAGE_WRITED` |
| 4    | `RES2`                      | unicast   | — (reserved)      | — |
| 5    | `PAGE_FILL_NEXT`            | unicast   | bootloader → host | `PAGE_FILL` |
| 6    | `PAGE_WRITED`               | unicast   | bootloader → host | next `PAGE_START` / `QUIT_BOOTLOADER` |
| 7    | `PAGE_FILL_BREAK`           | unicast   | bootloader → host | — |
| 8    | `COMMAND_ERROR`             | unicast   | node → host       | — |
| 9    | `REG_VALUE`                 | unicast   | node → host       | — |
| 10   | `SET_REG`                   | unicast   | host → node       | `REG_VALUE` / `COMMAND_ERROR` |
| 11   | `GET_REG`                   | unicast   | host → node       | `REG_VALUE` / `COMMAND_ERROR` |
| 12   | `SET_BIT`                   | unicast   | host → node       | `REG_VALUE` / `COMMAND_ERROR` |
| 13   | `CLEAR_BIT`                 | unicast   | host → node       | `REG_VALUE` / `COMMAND_ERROR` |
| 14   | `NODE_UPGRADE`              | unicast   | host → node       | `BOOTLOADER_TURNED_ON` (or `COMMAND_ERROR`) |
| 15   | `NODE_RESET`                | unicast   | host → node       | `NODE_TURNED_ON` after the restart |
| 16   | `DISCOVER`                  | broadcast to a group | host → nodes | `NODE_INFO` |
| 17   | `GROUP_RESET`               | broadcast to a group | host → nodes | `NODE_TURNED_ON` after the restart |
| 18   | `NODE_FAULT`                | broadcast | node              | — |
| 19   | `REG_VALUE_BROADCAST`       | broadcast | node              | — |
| 20   | `NODE_HEARTBEAT`            | broadcast | node              | — |
| 21   | `NODE_INFO`                 | broadcast | node              | — |
| 22   | `NODE_TURNED_ON`            | broadcast | node              | — |
| 23   | `BOOTLOADER_TURNED_ON`      | broadcast | bootloader        | `PAGE_START` |
| 24–31| `NODE_SPECIFIC_BROADCAST0–7`| broadcast | node              | — (defined by the node type) |

---

## Frame payloads

### Unicast — registers and errors

Register frames are described in detail in [`standard_registers.md`](standard_registers.md).

| Type | Payload |
|------|---------|
| `GET_REG`       | `[reg]` (dlc = 1) |
| `SET_REG`       | `[reg, value...]` — value big-endian, length = register size |
| `SET_BIT`       | `[reg, bit]` — set bit `bit` (0 = LSB) of the register |
| `CLEAR_BIT`     | `[reg, bit]` — clear bit `bit` |
| `REG_VALUE`     | `[reg, value...]` — register value after the operation; multi-frame if longer than 7 bytes |
| `COMMAND_ERROR` | `[error]` — `H9FRAME_ERROR_*`, see [Error codes](#error-codes) |

`SET_REG`, `SET_BIT` and `CLEAR_BIT` are answered with `REG_VALUE` carrying the register value
after the change, so the host always gets the actual state.

### Unicast — node control

| Type | Payload | Behaviour |
|------|---------|-----------|
| `NODE_RESET`   | none (dlc = 0) | The node restarts (AVR: watchdog → reset reason `WATCHDOG`; PIC: `RESET` instruction → `SOFTWARE`). |
| `NODE_UPGRADE` | none (dlc = 0) | The application jumps to the bootloader, see [`bootloader.md`](bootloader.md). An AVR library built without `BOOTSTART` answers `COMMAND_ERROR` / `BOOTLOADER_UNSUPPORTED`. |

Frames of the bootloader group (types 0–7) sent to an application are rejected with
`COMMAND_ERROR` / `INVALID_FRAME`.

### `DISCOVER` / `GROUP_RESET`

No payload; the broadcast group selects the nodes: a node reacts when the group equals its
node type or `0xFFFF`.

- `DISCOVER` — every selected node answers with `NODE_INFO`.
- `GROUP_RESET` — every selected node restarts (as with `NODE_RESET`).

### `NODE_INFO` / `NODE_TURNED_ON`

Same payload (dlc = 8); the broadcast group of the frame is the node type, it is not repeated
in the data. `NODE_TURNED_ON` is sent by the application once after start
(`CAN_send_turned_on_broadcast()`), `NODE_INFO` in response to `DISCOVER`.

| Byte | Content |
|------|---------|
| 0–3  | Firmware version, packed (see below) |
| 4    | PCB revision, ASCII letter (`'A'`, `'B'`, …) |
| 5    | BOM revision |
| 6–7  | Node flags, 16-bit, see [Node flags](#node-flags) (same as register 0) |

Packed version (also used by `BOOTLOADER_TURNED_ON`), big-endian 32-bit:

```
v     = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]
major = v >> 22             (10 bits, 0-1023)
minor = (v >> 11) & 0x7ff   (11 bits, 0-2047)
patch = v & 0x7ff           (11 bits, 0-2047)
```

Example: `00 80 00 00` → 2.0.0, `00 40 10 03` → 1.2.3.

### `BOOTLOADER_TURNED_ON`

See [`bootloader.md`](bootloader.md#bootloader_turned_on). dlc = 8: `[0..3]` bootloader version
(packed as above), `[4]` PCB revision, `[5]` BOM revision, `[6]` MCU type, `[7]` MCU clock.

### `NODE_FAULT`

Sent by a node when something went wrong; the group is the node type.

| Byte | Content |
|------|---------|
| 0    | Fault code |
| 1–7  | Optional, defined by the fault code |

| Code | Constant (`NODE_FAULT_*`) | Meaning |
|-----:|---------------------------|---------|
| 1    | `POWER_OUTAGE`            | Supply voltage dropped (e.g. detected by an analog comparator on the input voltage) |
| 2    | `CAN_FRAME_LOSS`          | A frame could not be sent: TX queue full while the controller is error passive / bus off |
| 3    | `CAN_RX_FRAME_LOSS`       | A received frame was dropped: receive buffer overflow |
| ≥ 4  | `NODE_SPECIFIC_FIRST_FAULT` + n | Faults defined by the node type |

`CAN_FRAME_LOSS` replaces the newest frame in the TX queue and goes out when the bus allows it
(after bus-off recovery at the latest). Both loss events are also recorded in the node flags.

### `REG_VALUE_BROADCAST`

Unsolicited register value, e.g. on a change: `[reg, value...]` as in `REG_VALUE` (single frame).
Example: the power switch publishes register 10 (status) whenever it changes.

### `NODE_HEARTBEAT`

Periodic "alive" frame; the payload is defined by the node type (may be empty).
Example: the power switch sends `[status]` every 30 s when the status does not change.

### `NODE_SPECIFIC_BROADCAST0–7`

Free for the node types (measurements, events…); the meaning is defined per node type.

---

## Multi-frame transfers

Register values longer than 7 bytes (up to `H9FRAME_MAX_REGISTER_SIZE` = 32 bytes, e.g. build
info or the AVR serial number) are sent as several `REG_VALUE` frames with the same `seqnum`:

| `flags` | Constant (`H9FRAME_FLAG_*`) | Payload |
|--------:|-----------------------------|---------|
| 0 | `SINGE_FRAME`        | `[reg, value...]` (up to 7 value bytes) |
| 1 | `MULTI_FRAME_FIRST`  | `[reg, frame_count, 6 value bytes]` |
| 2 | `MULTI_FRAME_MIDDLE` | `[reg, frame_index, 6 value bytes]` |
| 3 | `MULTI_FRAME_LAST`   | `[reg, frame_index, 1–6 value bytes]` |

- `frame_count` = number of frames, `ceil(length / 6)`; the first frame has index 0, the next
  ones carry `frame_index` 1, 2, …
- Value byte `i` of frame `n` is byte `6 * n + i` of the register value.
- The library builds these frames in `CAN_send_reg_value()`; h9d reassembles them.
- Non-zero `flags` are accepted only for `SET_REG` and `REG_VALUE`; any other unicast frame
  with `flags != 0` is rejected with `COMMAND_ERROR` / `INVALID_FRAME`. A multi-frame `SET_REG`
  is not reassembled by the library — its frames are passed to the application.

---

## Sequence numbers

- `seqnum` (0–31, wraps) exists only in unicast frames.
- The host (h9d) gives every request the next number.
- A node copies the request's `seqnum` into every response frame (`REG_VALUE`, all frames of a
  multi-frame value, `COMMAND_ERROR`); the host matches responses by source ID, type, register
  and `seqnum`.
- Bootloader responses (`PAGE_FILL_NEXT`, `PAGE_WRITED`, `PAGE_FILL_BREAK`) do not reliably
  echo it; hosts must not depend on their `seqnum`.
- Broadcast frames have no `seqnum` (those bits belong to the group).

---

## Node life cycle

```
power-on / reset                                   CAN bus
    │
    ├─ CAN_init(type, default_id, pcb, bom, version, build_info)
    │     node id from EEPROM (repairs a damaged copy) or default_id
    │     node flags: reset reason, bootloader presence / mismatch, default id
    │     hardware filters, 125 kbit/s
    ├─ enable interrupts
    ├─ CAN_send_turned_on_broadcast() ─────────────────► NODE_TURNED_ON
    │
    └─ main loop: CAN_get_msg()
          standard registers, DISCOVER, resets, upgrade → handled by the library
          registers ≥ 10, foreign responses, broadcasts  → returned to the application
```

- `DISCOVER` → `NODE_INFO`.
- `NODE_RESET` / `GROUP_RESET` → restart → `NODE_TURNED_ON` with the reset reason in the flags.
- `NODE_UPGRADE` → bootloader → `BOOTLOADER_TURNED_ON` (see [`bootloader.md`](bootloader.md)).

---

## Node flags

16-bit value sent in `NODE_INFO` / `NODE_TURNED_ON` (`data[6..7]`) and readable as register 0
(`NODE_FLAGS`). Masks: `NODE_FLAG_*` in `h9def.h`.

| Bits | Constant                       | Meaning |
|------|--------------------------------|---------|
| 0–2  | `NODE_FLAG_RESET_REASON_MASK`  | Reason for the last reset (`NODE_RESET_BY_*`, below) |
| 3    | `NODE_FLAG_BL_PRESENT`         | Bootloader present (its info block was found) |
| 4    | `NODE_FLAG_BL_MISMATCH`        | Bootloader built for another node type / PCB / BOM revision |
| 5    | `NODE_FLAG_DEFAULT_ID`         | No node ID in EEPROM, `default_id` is used |
| 6    | `NODE_FLAG_CAN_ERROR_WARNING`  | CAN error warning / error passive / bus off occurred since start |
| 7    | `NODE_FLAG_CAN_TX_FRAME_LOSS`  | A frame could not be sent since start (see `NODE_FAULT` 2) |
| 8    | `NODE_FLAG_CAN_RX_FRAME_LOSS`  | A received frame was dropped since start (see `NODE_FAULT` 3) |
| 9–15 | —                              | Reserved (0) |

Bits 0–5 are set in `CAN_init()`; bits 6–8 are sticky (cleared only by a reset). Bit 6 is
sampled in `CAN_get_msg()`, so a short error state between two calls can be missed.

| Reset reason | Constant (`NODE_RESET_BY_*`) | Meaning |
|-----:|---------------------|---------|
| 0 | `UNKNOWN`         | Not classified (AVR: several reset flags at once; PIC: MCLR, stack) |
| 1 | `POWER_ON`        | Power-on |
| 2 | `WATCHDOG`        | Watchdog (AVR: also `NODE_RESET` / `GROUP_RESET`) |
| 3 | `BROWN_OUT`       | Brown-out |
| 4 | `EXTERNAL_SOURCE` | External reset pin (AVR) |
| 5 | `SOFTWARE`        | `RESET` instruction (PIC: also `NODE_RESET` / `GROUP_RESET`) |

Bootloader detection (bits 3–4) reads the bootloader info block at the end of flash, see
[`bootloader.md`](bootloader.md#bootloader-info-block).

---

## Error codes

`COMMAND_ERROR` `data[0]` (`H9FRAME_ERROR_*`):

| Code | Constant                    | Meaning |
|-----:|-----------------------------|---------|
| 1    | `INVALID_FRAME`             | Frame not valid here (bootloader frame to an application, unexpected `flags`) |
| 2    | `BOOTLOADER_UNSUPPORTED`    | `NODE_UPGRADE`, but the node has no bootloader support |
| 3    | `UNSUPPORTED_OPERATION`     | Operation not supported for this register (e.g. `SET_BIT` on a standard register) |
| 4    | `UNSUPPORTED_REGISTER`      | Register not implemented by this node |
| 5    | `INVALID_REGISTER`          | Unknown register number (application registers) |
| 6    | `READ_ONLY_REGISTER`        | Write to a read-only register |
| 7    | `WRITE_ONLY_REGISTER`       | Read of a write-only register |
| 8    | `REGISTER_SIZE_MISMATCH`    | Wrong number of data bytes for the register |
| 9    | `INVALID_VALUE`             | Value out of the allowed range (e.g. node ID 0 or 0xFF) |
