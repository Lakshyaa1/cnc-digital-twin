# accel — KX134 Accelerometer

Overview
- Example project that reads the KX134 accelerometer over SPI and prints X/Y/Z acceleration in g.

Interface & Pins (as used in `accel/main/accel.c`)
- Interface: SPI
- MISO: GPIO19
- MOSI: GPIO23
- SCLK: GPIO18
- CS:   GPIO5

Notes
- SPI is initialized on `SPI2_HOST` with 1 MHz clock in the example.
- Adjust `PIN_NUM_*` defines in `accel/main/accel.c` if you wire different pins.

Datasheet
- Kionix KX134 datasheet: https://kionixfs.com/datasheets/KX134-1211.pdf
