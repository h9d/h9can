# H9 CAN Protocol

H9 uses 29-bit extended CAN frames at **250 kbit/s**. Each frame carries a
structured ID that encodes addressing and message type, plus up to 8 bytes of
payload.

---

## CAN ID layout (29 bits)

```
bit: 28   27–23   22–21   20–13    12–5           4–0
     ───   ─────   ─────   ──────   ────────────   ──────────
     prio  type    flags   src_id   dst_id         seqnum     ← unicast  (type bit 4 = 0)
     prio  type    flags   src_id   node_type[12:5] node_type[4:0]       ← broadcast (type bit 4 = 1)
```

| Field       | Bits | Range    | Description                              |
|-------------|------|----------|------------------------------------------|
| `priority`  | 1    | 0–1      | 0 = HIGH, 1 = LOW                        |
| `type`      | 5    | 0–31     | Message type (bit 4 distinguishes unicast/broadcast) |
| `flags`     | 2    | 0–3      | Multi-frame sequence counter (0 = single frame) |
| `source_id` | 8    | 0–255    | Sender's node ID                         |
| `dst_id`    | 8    | 0–255    | Recipient's node ID (unicast only)       |
| `seqnum`    | 5    | 0–31     | Request/response correlation number (unicast) |
| `node_type` | 13   | 0–8191   | Target node-type group (broadcast only)  |

The broadcast destination **0x1FFF** (`H9MSG_BROADCAST_ID`) matches all nodes
regardless of type.

---

## Message types

### Unicast (type 0x00–0x0F, type bit 4 = 0)

| Value | Constant                    | Direction     | Description                        |
|-------|-----------------------------|---------------|------------------------------------|
| 0x00  | `RES1`                      | —             | Reserved                           |
| 0x01  | `PAGE_START`                | host → node   | Begin flashing a page              |
| 0x02  | `QUIT_BOOTLOADER`           | host → node   | Exit bootloader, jump to app       |
| 0x03  | `PAGE_FILL`                 | host → node   | 8 bytes of page data               |
| 0x04  | `BOOTLOADER_TURNED_ON`      | node → bcast  | Bootloader announce (sent as unicast to 0xFF) |
| 0x05  | `PAGE_FILL_NEXT`            | node → host   | Ready for next PAGE_FILL           |
| 0x06  | `PAGE_WRITED`               | node → host   | Page committed to flash            |
| 0x07  | `PAGE_FILL_BREAK`           | node → host   | Page fill aborted                  |
| 0x08  | `COMMAND_ERROR`             | node → host   | Error response                     |
| 0x09  | `SET_REG`                   | host → node   | Write register value               |
| 0x0A  | `GET_REG`                   | host → node   | Read register value                |
| 0x0B  | `SET_BIT`                   | host → node   | Set bit(s) in register             |
| 0x0C  | `CLEAR_BIT`                 | host → node   | Clear bit(s) in register           |
| 0x0D  | `NODE_UPGRADE`              | host → node   | Enter bootloader (dlc = 0)         |
| 0x0E  | `NODE_RESET`                | host → node   | Restart node via watchdog          |
| 0x0F  | `REG_VALUE`                 | node → host   | Register value response            |

### Broadcast (type 0x10–0x1F, type bit 4 = 1)

| Value | Constant                    | Sender | Description                               |
|-------|-----------------------------|--------|-------------------------------------------|
| 0x10  | `DISCOVER`                  | host   | Ask nodes in group to report              |
| 0x11  | `GROUP_RESET`               | host   | Reset all nodes in group                  |
| 0x12  | `NODE_FAULT`                | node   | Node reports a fault                      |
| 0x13  | `REG_VALUE_BROADCAST`       | node   | Broadcast a register value                |
| 0x14  | `NODE_HEARTBEAT`            | node   | Periodic heartbeat                        |
| 0x15  | `NODE_INFO`                 | node   | Response to DISCOVER                      |
| 0x16  | `NODE_TURNED_ON`            | node   | Node finished startup                     |
| 0x17  | `RES2`                      | —      | Reserved                                  |
| 0x18–0x1F | `NODE_SPECIFIC_BROADCAST0–7` | node | Application-defined broadcasts       |

---

## Node addressing

Node IDs are 8-bit values (1–254). ID 0 is reserved; ID 255 (0xFF) is used by
the bootloader as a broadcast destination for `BOOTLOADER_TURNED_ON`.

Node IDs are stored in EEPROM and read on startup by `CAN_init()`. They can be
changed at runtime via register 9 (`NODE_ID_STD_REGISTER`); the new value takes
effect after the next reset.

---

## Node lifecycle

### Startup sequence

```
Application         CAN bus
    │
    ├─ CAN_init(type, rev, version, build_info)
    │     reads node_id from EEPROM
    │     configures CAN peripheral and message filters
    │
    ├─ sei()
    ├─ _delay_ms(100)
    ├─ CAN_send_turned_on_broadcast() ──────────────► NODE_TURNED_ON (broadcast)
    │
    └─ main loop: CAN_get_msg() ...
```

### NODE_TURNED_ON / NODE_INFO payload

Both messages carry the same 8-byte payload:

```
data[0..1]  node_type    (uint16, big-endian)
data[2..3]  version_major (uint16, big-endian)
data[4..5]  version_minor (uint16, big-endian)
data[6]     hardware_revision (ASCII char, e.g. 'a')
data[7]     reset_reason  (see NODE_RESET_BY_* in h9def.h)
```

### Discover

A host sends `DISCOVER` with a broadcast `node_type` group. Every matching node
responds with `NODE_INFO` carrying the same payload as `NODE_TURNED_ON`.
Setting group to `H9MSG_BROADCAST_ID` (0x1FFF) reaches all nodes.

### Reset

`NODE_RESET` (unicast, dlc = 0) → node restarts via watchdog.

---

## Register access

See `doc/standard_registers.md` for the full register reference.

Registers 0–9 are handled automatically by the `can.c` library (standard
registers). Registers ≥ 10 are passed back to the application via `CAN_get_msg()`
returning 1.

```
GET_REG  data[0] = register_number             dlc = 1
SET_REG  data[0] = register_number, data[1..] = value
REG_VALUE data[0] = register_number, data[1..] = value   (response)
COMMAND_ERROR data[0] = error_code             (on any error)
```

The `flags` field in the CAN ID is reserved for multi-frame register transfers
(e.g. long strings). Non-zero `flags` is currently only accepted for `SET_REG`
and `REG_VALUE`; all other message types reject it with `COMMAND_ERROR /
H9FRAME_ERROR_INVALID_MSG`.

---

## Bootloader protocol

The bootloader is a separate firmware image residing in the MCU's boot section.
It is entered when the application receives `NODE_UPGRADE` (unicast, dlc = 0).
The application jumps to `BOOTSTART` (flash address defined at compile time).

### Entering the bootloader

```
Host                                Node (application)
 │                                        │
 ├─── NODE_UPGRADE (dlc=0) ──────────────►│
 │                                        ├─ cli()
 │                                        ├─ jmp BOOTSTART
 │                                        │       ↓
 │                                  Node (bootloader)
 │                                        ├─ relocate interrupt vectors to boot section (IVSEL=1)
 │                                        ├─ CAN_init()
 │◄── BOOTLOADER_TURNED_ON (bcast) ───────┤
```

`BOOTLOADER_TURNED_ON` is sent as a unicast frame with `dst_id = 0xFF` and
`priority = HIGH`. It is re-sent periodically if no PAGE_START is received
(keepalive).

```
data[0]  BOOTLOADER_VERSION_MAJOR
data[1]  BOOTLOADER_VERSION_MINOR
data[2]  MCU type (NODE_MCU_* enum)
data[3]  MCU frequency (NODE_MCU_F_* enum)
dlc = 4
```

### Flashing a page

One exchange per flash page. Page numbering starts at 0; each page is
`SPM_PAGESIZE` bytes (MCU-dependent, typically 128 or 256 bytes).

```
Host                                Node (bootloader)
 │                                        │
 ├─── PAGE_START  data=[page_hi, page_lo] ►│  dlc=2, page number big-endian
 │◄── PAGE_FILL_NEXT data=[rem_hi,rem_lo] ─┤  bytes remaining = SPM_PAGESIZE
 │                                        │
 ├─── PAGE_FILL   data=[b0..b7]  ─────────►│  dlc=8, 8 bytes of page data
 │◄── PAGE_FILL_NEXT data=[rem_hi,rem_lo] ─┤  bytes remaining decreases by 8
 │                                        │
 ├─── PAGE_FILL   ...                     │  repeat until remaining = 0
 │◄── PAGE_WRITED data=[addr_hi, addr_lo] ─┤  page committed; addr = page*SPM_PAGESIZE
```

`PAGE_FILL` data is packed as little-endian 16-bit words:

```
data[0] = word0_lo, data[1] = word0_hi
data[2] = word1_lo, data[3] = word1_hi
...
```

If the host sends any bootloader-type message (type 0x00–0x07) during a fill
sequence the bootloader aborts with `PAGE_FILL_BREAK` (dlc = 0).

All bootloader frames use `priority = HIGH`.

### Exiting the bootloader

```
Host                                Node (bootloader)
 │                                        │
 ├─── QUIT_BOOTLOADER (dlc=0) ───────────►│
 │                                        ├─ IVSEL = 0  (restore interrupt vectors)
 │                                        └─ jmp 0x0000
```

---

## Application API (`include/avr/can.h`)

```c
/* Initialise CAN peripheral, read node ID from EEPROM, configure filters.
 * Call before sei(). */
void CAN_init(uint16_t node_type, char hardware_rev,
              uint16_t version_major, uint16_t version_minor,
              const char *build_info);

/* Send NODE_TURNED_ON broadcast. Call after sei() and a short delay. */
void CAN_send_turned_on_broadcast(void);

/* Non-blocking send.
 *   returns 1  – sent immediately via MOb 0
 *   returns 2  – queued in TX ring buffer (depth 8)
 *   returns 0  – TX buffer full, frame dropped */
uint8_t CAN_put_msg(h9msg_t *cm);

/* Non-blocking send attempt directly to MOb 0 (no buffering).
 *   returns 1 – sent, 0 – MOb busy */
uint8_t CAN_try_put_msg(h9msg_t *cm);

/* Non-blocking receive. Internally handles standard registers (0–9),
 * DISCOVER, NODE_RESET, NODE_UPGRADE, and bootloader rejection.
 *   returns 1 – message in *cm is for the application (register ≥ 10 or
 *               REG_VALUE / COMMAND_ERROR from a remote node)
 *   returns 0 – message handled internally, nothing for the application */
uint8_t CAN_get_msg(h9msg_t *cm);

/* Populate response fields (type, seqnum, source/dest) from a received request.
 * Sets res->type to REG_VALUE for SET/GET/SET_BIT/CLEAR_BIT requests, and to
 * NODE_INFO for DISCOVER. */
void CAN_init_response_msg(const h9msg_t *req, h9msg_t *res);

/* Send COMMAND_ERROR with the given error code. */
void send_command_error(uint8_t errno, uint8_t destination, uint8_t seqnum);

/* Configure optional receive filters (MOb 4 and MOb 5).
 * Each filter can match by remote node ID and/or broadcast group. */
void CAN_set_msg_filter_1(uint8_t remote_node_id, uint8_t remote_node_id_active,
                           uint16_t broadcast_group, uint8_t broadcast_group_active);
void CAN_set_msg_filter_2(uint8_t remote_node_id, uint8_t remote_node_id_active,
                           uint16_t broadcast_group, uint8_t broadcast_group_active);
```

### Global

```c
volatile uint8_t can_node_id;   /* current node ID, set by CAN_init() */
```

### Message filters

The library permanently occupies three hardware message objects (MObs):

| MOb | Purpose                                           |
|-----|---------------------------------------------------|
| 0   | TX (transmit)                                     |
| 1   | Unicast RX – frames addressed to `can_node_id`   |
| 2   | Special broadcast RX – all nodes (`DISCOVER`, `GROUP_RESET`) |
| 3   | Special broadcast RX – frames to this node's type group |
| 4   | Optional filter 1 (`CAN_set_msg_filter_1`)        |
| 5   | Optional filter 2 (`CAN_set_msg_filter_2`)        |

---

## Error codes (`H9FRAME_ERROR_*`)

| Value | Constant                              | Meaning                                      |
|-------|---------------------------------------|----------------------------------------------|
| 1     | `H9FRAME_ERROR_INVALID_MSG`           | Message type not valid in current node state |
| 2     | `H9FRAME_ERROR_BOOTLOADER_UNSUPPORTED`| NODE_UPGRADE requested, no bootloader linked |
| 3     | `H9FRAME_ERROR_UNSUPPORTED_OPERATION` | Operation not supported for this register    |
| 4     | `H9FRAME_ERROR_INVALID_REGISTER`      | Register number unknown                      |
| 5     | `H9FRAME_ERROR_READ_ONLY_REGISTER`    | Write attempted on a read-only register      |
| 6     | `H9FRAME_ERROR_WRITE_ONLY_REGISTER`   | Read attempted on a write-only register      |
| 7     | `H9FRAME_ERROR_REGISTER_SIZE_MISMATCH`| Wrong payload length for this register       |
