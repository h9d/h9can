# h9can

h9can is a monorepo for the common part of the h9 project. It contains an implementation of the h9can protocol, a bootloader for h9 nodes and other useful stuff:)

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

```shell
  cmake -S avr_bootloader -B build-bootloader
  cmake --build build-bootloader

  #Buduje wszystkie 15 wariantów. Żeby sflashować ten konkretny (atmega32m1 @ 16MHz jest domyślny, więc nie trzeba nic zmieniać):

  cmake --build build-bootloader --target flash-bootloader
  
  #Jeśli chcesz inny wariant, np. atmega64m1 @ 12MHz:

  cmake -S avr_bootloader -B build-bootloader -DAVR_MCU=atmega64m1 -DAVR_F_CPU=12000000
  cmake --build build-bootloader --target flash-bootloader
```

# Test

```shell
cmake --build . --target test
```
