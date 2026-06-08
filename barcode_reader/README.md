# barcode_reader — UART Barcode Scanner

Overview
- Example that reads bytes from a UART-connected barcode scanner and prints the raw bytes and ASCII representation.

Interface & Pins (as used in `barcode_reader/main/barcode_reader.c`)
- Interface: UART (UART2)
- TX: GPIO17
- RX: GPIO16
- Baud: 9600 (example config)

Notes
- The example installs a UART driver and reads bytes with a 200 ms timeout.
- Configure baud and pins in `barcode_reader/main/barcode_reader.c` to match your scanner.

Datasheet / Reference
- Example scanner manual (provided): https://files.waveshare.com/wiki/Barcode-Scanner-Module/Barcode_Scanner_Module_Setting_Manual_V2.1.pdf
