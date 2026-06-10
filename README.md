# Digital Twin of a CNC machine
This workspace contains sensor and peripheral example projects for ESP32 using ESP-IDF.

**Final Goal / Project Vision**

The end goal for this workspace is a predictive maintenance system for a CNC machine using the sensors and examples in this repository. High-level objectives:
- Collect vibration (accelerometer), temperature & pressure (BMP280), ultrasonic distance, analog pressure, water detection, and RFID/barcode events from the machine.
- Build a digital twin of the CNC machine that mirrors its physical state using the sensor streams.
- Publish sensor readings periodically to a backend or broker (MQTT / HTTP) and store them for real-time monitoring and historical analysis.
- Apply analytics and ML models for anomaly detection and remaining useful life (RUL) estimation to enable predictive maintenance alerts.
- Provide a dashboard that visualizes live telemetry, historical trends, and alerts.

 In progress:
- Add wiring diagrams / pin mappings for each example so hardware can be reproduced reliably.
- Add a small data-publisher example (MQTT or HTTP) for one component to demonstrate telemetry flow.
- Sketch a minimal digital-twin data model and dashboard mockup (Grafana/Node-RED/React).

**ESP-IDF Version:** 5.5.4 (see `sdkconfig` files)

**Components & Sensors (by folder):**
- `accel`: KX134 accelerometer - interface: SPI (uses `spi_master`).
- `barcode_reader`: UART barcode scanner - interface: UART (UART2).
- `basics`: BMP280 temperature & pressure sensor - interface: I2C (BMP280 over I2C).
- `new_pressure`: Analog pressure sensor - interface: ADC (ADC1 channel, GPIO34).
- `RFID`: RC522 RFID reader - interface: SPI (RC522 protocol / ISO14443A).
- `RFID_Open`: RC522 examples and driver wrapper - interface: SPI (examples also include I2C example in managed component examples).
- `Ultrasonic`: Ultrasonic distance sensor (e.g. HC-SR04 style) - interface: GPIO + RMT peripheral (trigger/echo timing).
- `water_sensor`: Simple digital water detection - interface: GPIO digital input (GPIO4 in example).
- `trials`: Misc experiments and build artifacts (various driver usage).

**Communication protocols used across the workspace:**
- SPI
- I2C
- UART
- ADC (analog readings)
- RMT / GPIO pulse timing
- GPIO digital input

How to find details:
- Source files for each component are in the component folder under `main/` (e.g., `accel/main/accel.c`).
- The `sdkconfig` files at each component root include the ESP-IDF project configuration and show the ESP-IDF version header.

