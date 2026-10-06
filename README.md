# h9can

h9can is a monorepo for the common part of the h9 project. It contains an implementation of the h9can protocol, a bootloader for h9 nodes and other useful stuff:)

# Documentation

- [`doc/protocol.md`](doc/protocol.md) — H9 CAN protocol: identifiers, frame types and payloads, node flags, errors
- [`doc/standard_registers.md`](doc/standard_registers.md) — standard registers 0–9
- [`doc/bootloader.md`](doc/bootloader.md) — bootloader, firmware upgrade protocol, bootloader info block
- [`doc/library.md`](doc/library.md) — using the library in a node (AVR / PIC)
- [`doc/nodes.md`](doc/nodes.md) — node types
- [`doc/SN.md`](doc/SN.md) — PIC serial number
- [`include/h9can.h`](include/h9can.h) — library API

# Create new AVR project
```shell
PROJECT_NAME=power_switch
mkdir $PROJECT_NAME
cd $PROJECT_NAME
git init .
git submodule add git@github.com:h9d/h9can.git h9can
mkdir src
sed "s/PROJECT_NAME/${PROJECT_NAME}/" h9can/template/avr_CMakeLists.txt > CMakeLists.txt
cp h9can/template/avr_main.c src/main.c
cp h9can/template/version.h.in src/
```

# Bootloader

The node type and the PCB / BOM revision are compiled into the bootloader (see [`doc/bootloader.md`](doc/bootloader.md)):

```shell
cmake -S avr_bootloader -B build-bootloader -DNODE_TYPE=0x0102 -DPCB_REVISION=B -DBOM_REVISION=1
cmake --build build-bootloader                              # all MCU x frequency variants
cmake --build build-bootloader --target flash-bootloader    # atmega32m1 @ 16 MHz by default

# another variant, e.g. atmega64m1 @ 12 MHz:
cmake -S avr_bootloader -B build-bootloader -DAVR_MCU=atmega64m1 -DAVR_F_CPU=12000000
cmake --build build-bootloader --target flash-bootloader
```

A node project built from `template/` builds its bootloader automatically (`flash-all` writes bootloader + application).

# Test

```shell
cmake --build . --target test
```
