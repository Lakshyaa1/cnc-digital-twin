# RFID — RC522 RFID Reader (SPI)

Overview
- Example RC522 driver that talks to an MFRC522-compatible RFID reader over SPI and polls for ISO14443A cards.

Interface & Pins (as used in `RFID/main/RFID.c`)
- Interface: SPI
- MOSI: GPIO23
- MISO: GPIO19
- SCLK: GPIO18
- CS (SDA): GPIO5

Notes
- The example uses 1 MHz SPI and configures the RC522 to force 100% ASK modulation and enable the RF field.

Datasheet
- NXP MFRC522 datasheet: https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf
