# nrf24lu1-dfu-bootloader

A DFU-compatible bootloader for the NRF24LU1/NRF24LU1+ based USB dongles and boards.

## Requirements

- SDCC (minimum version 3.1.0)
- GNU Binutils

## Supported Hardware

The following hardware has been tested and is known to work.

- CrazyRadio PA USB dongle
- SparkFun nRF24LU1+ breakout board
- Partially: Logitech Unifying dongle (model C-U0007, Nordic Semiconductor based)

## Build the firmware

```
make
```

## Using the bootloader

Once the bootloader is flashed to the device, it runs on device powerup
for a few seconds before handing over control to the payload.
During this time it dfu-util can be used to flash update the payload or
the bootloader itself.

```
$ sudo dfu-util --list
dfu-util 0.9

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2016 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

Found DFU: [1915:0102] ver=0001, devnum=50, cfg=1, intf=0, path="6-2.1.3.4", alt=1, name="boot", serial="UNKNOWN"
Found DFU: [1915:0102] ver=0001, devnum=50, cfg=1, intf=0, path="6-2.1.3.4", alt=0, name="flash", serial="UNKNOWN"
```

The payload is required to start with an LJMP as the first instruction (as
this will be patched while flashing to jump to the bootloader).
