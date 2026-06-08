# basics — BMP280 Temperature & Pressure Sensor (I2C)

Overview
- Example that communicates with a BMP280 sensor over I2C to read temperature and pressure.

Interface & Pins (as used in `basics/main/basics.c`)
- Interface: I2C (master)
- I2C Port: `I2C_NUM_0`
- SDA: GPIO21
- SCL: GPIO22
- BMP280 I2C address used in example: 0x76

Notes
- Example uses `i2c_master` helper API and configures the bus with internal pull-ups enabled.
- If your BMP280 module uses address 0x77, update `device_address` accordingly.

Datasheet
- Bosch BMP280 datasheet: https://cdn-shop.adafruit.com/datasheets/BST-BMP280-DS001-10.pdf
