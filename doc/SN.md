## PIC Serial number

Numer seryjny wpisujemy do User ID memory jak ponizej, jest tam 8 bajtow.
Zerowy (0x200000) rezerwujemy na hardware version, sn jest od 1 (0x200001).

```
cat >> id.hex << EOF 
:020000040020DA
:0800000012345678AABBCCDDD6
:00000001FF
EOF
/Applications/microchip/mplabx/v6.30/mplab_platform/mplab_ipe/bin/ipecmd.sh -TPPK4 -P18F46K80 -Fid.hex -MI -OL -OH
```
