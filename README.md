# koti-node

Koti-node is a firmware for individual node devices in my personal home
automation system.

A node is a physical device (based on nRF52 chip) that can have multiple
sensors connected to it. The node advertises sensor data as a Bluetooth
low-energy (BLE) beacon, which is then read by the central hub -
[koti-hub](https://github.com/jarijokinen/koti-hub).

## Features

* Reads analog value from AIN0 (GPIO 0.02) and advertises it

BLE advertisement data format is 0x2939010000 where first two bytes (2939) are
just my personal identifier, next byte (01) is zone/room identifier, last two
bytes (0000) are the ambient light value (0-4095) from the phototransistor
connected to AIN0.

## Requirements

* nRF52840 SoC
* nRF5 SDK and GNU toolchain installed

## License

MIT License. Copyright (c) 2020 [Jari Jokinen](https://jarijokinen.com).  See
[LICENSE](https://github.com/jarijokinen/koti-node/blob/master/LICENSE.txt) for
further details.
