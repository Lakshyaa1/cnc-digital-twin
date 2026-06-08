# RFID_Open — RC522 Examples & Driver Wrapper

Overview
- Wrapper and higher-level examples around the RC522 driver. This folder demonstrates a driver-based approach with event callbacks, scanner instances, and managed examples.

Interface & Pins (as used in `RFID_Open/main/RFID_Open.c`)
- Interface: SPI (driver-configurable)
- MOSI: GPIO23
- MISO: GPIO19
- SCLK: GPIO18
- CS (SDA): GPIO5
- RST: tied to 3.3V in examples (or set a GPIO if your board exposes reset)

Notes
- The `RFID_Open` project builds on a reusable `rc522` component and registers PICC (card) state change events for higher-level logic.

Datasheet
- NXP MFRC522 datasheet: https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf
