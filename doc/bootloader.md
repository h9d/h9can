# H9 Bootloader

Every node can carry a bootloader in the boot area of its flash. It updates the application
firmware over CAN and identifies the board (node type, PCB / BOM revision) independently of
the application.

Two implementations share the protocol:

| | AVR (`avr_bootloader/`) | PIC (`pic_bootloader/`) |
|---|---|---|
| MCU | ATmega16M1, 32M1, 64M1, 32C1, AT90CAN128 | PIC18F46K80 |
| Location | boot section, 2 KB (`bootstart_<mcu>` in `cmake/avr_alt_setting.cmake`) | 0xF600–0xFFFF (2.5 KB) |
| Flash page / block | `SPM_PAGESIZE`: 128 B (16M1, 32M1, 32C1), 256 B (64M1, AT90CAN128) | 64 B (erase block) |
| Size (approx.) | 1.74–1.83 KB of 2 KB | 2.4 KB of 2.5 KB |

Frame layouts: [`protocol.md`](protocol.md).

---

## Entering and leaving the bootloader

```
Host                                 Node (application)
 │                                         │
 ├─── NODE_UPGRADE (dlc = 0) ─────────────►│ interrupts off
 │                                         │ AVR: jmp BOOTSTART    PIC: goto 0xF600
 │                                   Node (bootloader)
 │                                         │ AVR: interrupt vectors → boot section (IVSEL)
 │                                         │ CAN init (node ID from EEPROM, 0 if none)
 │◄── BOOTLOADER_TURNED_ON ────────────────┤ repeated until the host answers
 │          ...  flashing  ...             │
 ├─── QUIT_BOOTLOADER (dlc = 0) ──────────►│ AVR: IVSEL = 0, jmp 0x0000   PIC: RESET
 │                                         │
 │◄── NODE_TURNED_ON ──────────────────────┤ application started
```

- The application enters the bootloader on `NODE_UPGRADE`. An AVR library built without
  `BOOTSTART` (it is set by `avr/CMakeLists.txt`) answers `COMMAND_ERROR` /
  `BOOTLOADER_UNSUPPORTED` instead.
- A reset always starts the application (AVR: `BOOTRST` fuse unprogrammed).
- The bootloader only receives frames of the bootloader group (types 0–7) addressed to its
  node ID; it ignores everything else on the bus.
- While waiting it re-sends `BOOTLOADER_TURNED_ON` after each receive timeout (a busy-wait loop,
  about 1–2 s); it stays in the bootloader until `QUIT_BOOTLOADER`.

---

## BOOTLOADER_TURNED_ON

Broadcast, the group is the node type compiled into the bootloader. dlc = 8:

| Byte | Content |
|------|---------|
| 0–3  | Bootloader version, packed big-endian 32-bit: major bits 31–22, minor 21–11, patch 10–0 ([decoding](protocol.md#node_info--node_turned_on)) |
| 4    | PCB revision, ASCII letter |
| 5    | BOM revision |
| 6    | MCU type (`NODE_MCU_*`, see [register 5](standard_registers.md#register-5--node_mcu_type)) |
| 7    | MCU clock (`NODE_MCU_F_*`, below) |

| Value | Constant (`NODE_MCU_F_*`) |
|------:|---------------------------|
| 1 | `2MHz` |
| 2 | `4MHz` |
| 3 | `6MHz` |
| 4 | `8MHz` |
| 5 | `12MHz` |
| 6 | `16MHz` |

The host checks the MCU type, clock, node type and revisions before uploading a firmware image.

---

## Flashing

The image is written page by page (AVR: `SPM_PAGESIZE`, PIC: 64-byte block), starting at
address 0. One exchange per page:

```
Host                                         Node (bootloader)
 │                                                 │
 ├─── PAGE_START     [n_hi, n_lo] ────────────────►│ page / block n erased
 │◄── PAGE_FILL_NEXT [rem_hi, rem_lo] ─────────────┤ rem = page size
 ├─── PAGE_FILL      [b0 ... b7] ─────────────────►│ 8 bytes of the image
 │◄── PAGE_FILL_NEXT [rem_hi, rem_lo] ─────────────┤ rem -= 8
 │            ...  until rem = 0  ...              │
 │◄── PAGE_WRITED    [a_hi, a_lo] ─────────────────┤ page written
 ├─── PAGE_START     [n+1] ...                     │
```

| Frame | Payload |
|-------|---------|
| `PAGE_START` (1)     | `[n_hi, n_lo]`, dlc = 2 — page number (AVR) / block number (PIC), byte address = n × page size |
| `PAGE_FILL_NEXT` (5) | `[rem_hi, rem_lo]` — bytes still missing in the current page |
| `PAGE_FILL` (3)      | exactly 8 bytes of the image, in flash order (dlc = 8) |
| `PAGE_WRITED` (6)    | AVR: `[a_hi, a_lo]` — byte address of the page (low 16 bits, so it wraps above 64 KB on AT90CAN128); PIC: `[n_hi, n_lo]` — block number |
| `PAGE_FILL_BREAK` (7)| dlc = 0 — the page was abandoned |
| `QUIT_BOOTLOADER` (2)| dlc = 0 — start the application |

- `PAGE_START` erases the page.
- Any other bootloader frame from the host during a page fill (including a `PAGE_FILL` with
  dlc ≠ 8) aborts it with `PAGE_FILL_BREAK`; the page stays erased, the host starts it again
  with `PAGE_START`.
- The bootloader answers only the node that sent `PAGE_START`.
- The `seqnum` of bootloader responses is not meaningful.
- The image must not overlap the bootloader area (the bootloader does not protect itself; the
  AVR boot section can be locked with the `BLB1x` lock bits).

The h9d tool `h9fwupload` implements the host side: it waits for `BOOTLOADER_TURNED_ON` from
the node, uploads the pages in order and sends `QUIT_BOOTLOADER` after the last one.

---

## Bootloader info block

The bootloader stores its identity in the **last 10 bytes of flash**, so the application can
read it and compare it with its own values (node flags `BL_PRESENT` / `BL_MISMATCH`).

| Offset | Size | Field |
|-------:|-----:|-------|
| 0 | 2 | `magic` = 0x4839 (`'H' '9'`), little-endian |
| 2 | 2 | `node_type`, little-endian |
| 4 | 1 | `pcb_rev` (ASCII letter) |
| 5 | 1 | `bom_rev` |
| 6 | 4 | `version` packed (10 / 11 / 11 bits), little-endian |

| | AVR | PIC |
|---|---|---|
| Header | `include/h9avr/bl_info.h` | `include/h9pic/bl_info.h` |
| Address | `FLASHEND + 1 − 10`: 0x3FF6 (16M1), 0x7FF6 (32M1 / 32C1), 0xFFF6 (64M1), 0x1FFF6 (AT90CAN128) | `_ROMSIZE − 10` = 0xFFF6 |
| Placement | section `.blinfo`, linker `--section-start` (`flashend_<mcu>` in `cmake/avr_alt_setting.cmake`) | `__at(H9_BL_INFO_ADDR)` |
| Read by the application | `read_bl_info()` — `memcpy_P` / `memcpy_PF` (LPM / ELPM) | `read_bl_info()` — `TBLRD` |

`read_bl_info()` returns 0 when there is no bootloader, the bootloader is older than the block,
or the boot area is read-protected (AVR `BLB12`, PIC `EBTRx`); the node then reports
`BL_PRESENT` = 0.

---

## Building

The node identity is compiled into the bootloader and is **required** at configure time:

| Variable       | Description | Example |
|----------------|-------------|---------|
| `NODE_TYPE`    | Node type (broadcast group) | `0x0102` |
| `PCB_REVISION` | PCB revision, a letter `A`–`Z` | `B` |
| `BOM_REVISION` | BOM revision (0–255) | `1` |

### AVR

```shell
cmake -S avr_bootloader -B build-bootloader \
      -D NODE_TYPE=0x0102 -D PCB_REVISION=B -D BOM_REVISION=1 \
      -D AVR_MCU=atmega32m1 -D AVR_F_CPU=16000000
cmake --build build-bootloader                              # all MCU x frequency variants
cmake --build build-bootloader --target flash-bootloader    # AVR_MCU @ AVR_F_CPU
```

- Without one of the variables the configuration succeeds, but building the bootloader fails
  (an application project that includes h9can still configures).
- An application project (`cmake/avr.cmake`) builds the bootloader for its MCU with its own
  `NODE_TYPE` / `PCB_REVISION` / `BOM_REVISION` and offers `flash-all` (bootloader +
  application). The `flash` target erases the chip and writes **only the application** — the
  bootloader is lost.
- Fuses (`fuse` target, `cmake/avrdude-helpers.cmake`): `BOOTSZ` = 1024 words (2 KB, matches
  `bootstart_<mcu>`), `EESAVE` programmed (EEPROM — node ID — survives chip erase),
  BOD enabled (4.5 V on the M1 family).

See also [`avr_bootloader/README.md`](../avr_bootloader/README.md).

### PIC

```shell
cmake --preset h9pic_bootloader_default_conf -S pic_bootloader/cmake/h9pic-bootloader/default \
      -D NODE_TYPE=0x0102 -D PCB_REVISION=B -D BOM_REVISION=1
cmake --build pic_bootloader/_build/h9pic-bootloader/default
```

- The configuration fails without the variables.
- XC8 drops the quotes of a char literal passed with `-D`, so `PCB_REVISION` is passed as its
  ASCII code (`B` → `0x42`).
- The bootloader is linked at 0xF600 (code offset in the MPLAB project); the device
  configuration bits are in `pic_bootloader/config.h`.

See also [`pic_bootloader/README.md`](../pic_bootloader/README.md).
