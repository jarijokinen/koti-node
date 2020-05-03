# koti-node

Koti-node is a firmware for individual node devices in my personal home
automation system.

A node is a physical device (based on nRF52 chip) that can have multiple
sensors connected to it. The node advertises sensor data as a Bluetooth
low-energy (BLE) beacon, which is then read by the central hub -
[koti-hub](https://github.com/jarijokinen/koti-hub).

## Requirements

* nRF52840 SoC
* nRF5 SDK and GNU toolchain installed

## License

MIT License. Copyright (c) 2020 [Jari Jokinen](https://jarijokinen.com).  See
[LICENSE](https://github.com/jarijokinen/koti-node/blob/master/LICENSE.txt) for
further details.
