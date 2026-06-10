# Digital Twin of a CNC Machine

This repository contains ESP32-based sensor modules and an integrated FreeRTOS firmware that together form the foundation of a Digital Twin and Predictive Maintenance system for a CNC machine.

The objective is to collect real-time machine data, maintain a digital representation of the machine's state, and build the infrastructure required for monitoring, analytics, anomaly detection, and predictive maintenance.

---

## Project Vision

The long-term goal of this project is to develop a complete Digital Twin of a CNC machine capable of:

- Monitoring machine behaviour in real time using multiple sensors.
- Building a digital representation of the machine's operating state.
- Publishing telemetry data through MQTT and HTTP.
- Storing historical data for analysis and visualization.
- Detecting abnormal operating conditions using analytics and machine learning.
- Estimating machine health and Remaining Useful Life (RUL).
- Generating maintenance alerts before failures occur.
- Providing a dashboard for operators and maintenance personnel.

---

## Current Status

### Completed

- KX134 accelerometer integration for vibration monitoring.
- BMP280 integration for temperature and pressure sensing.
- Analog pressure sensor integration.
- Ultrasonic distance measurement.
- Water/leak detection.
- RFID authentication using RC522.
- Barcode scanner integration.
- FreeRTOS-based multi-task architecture.
- Centralized sensor data aggregation.
- HTTP telemetry server.
- MQTT communication framework and RTOS integration.

### In Progress

- Sensor wiring diagrams and pin mapping documentation.
- MQTT telemetry publishing for all sensor streams.
- Digital Twin state model.
- Grafana/Node-RED dashboard integration.
- Machine health scoring system.
- FFT-based vibration analysis.

### Planned

- Anomaly detection algorithms.
- Remaining Useful Life (RUL) estimation.
- Predictive maintenance alerts.
- Historical telemetry storage.
- Cloud integration and remote monitoring.

---

## Repository Structure

### Sensor Modules

| Folder | Description | Interface |
|----------|------------|------------|
| accel | KX134 accelerometer for vibration monitoring | SPI |
| basics | BMP280 temperature and pressure sensor | I2C |
| new_pressure | Analog pressure sensor | ADC |
| Ultrasonic | Ultrasonic distance sensor | GPIO + RMT |
| water_sensor | Water/leak detection sensor | GPIO |
| RFID_Open | Primary RC522 RFID implementation used in the integrated system | SPI |
| RFID | Earlier RC522 experiments and reference code | SPI |
| barcode_reader | UART barcode scanner | UART |
| trials | Experimental code and testing projects | Various |

> Note: Both RFID and RFID_Open are present in the repository. The integrated firmware currently uses the implementation from RFID_Open as the primary RFID solution.

### Integrated Firmware

The integrated_rtos_work project combines all supported sensors into a single FreeRTOS-based firmware and serves as the main development branch for the Digital Twin system.

Current tasks include:

- Accelerometer Task
- Pressure Task
- Ultrasonic Task
- RFID Task
- Barcode Task
- Aggregator Task
- Analytics Task
- Housekeeping Task
- WiFi Server Task
- MQTT Task

Sensor readings are collected independently and merged into a shared machine state by the aggregation layer.

---

## Communication Protocols

The project currently uses:

- SPI
- I2C
- UART
- ADC
- GPIO
- RMT
- WiFi
- HTTP
- MQTT

---

## System Architecture

text Physical CNC Machine         │         ▼ ESP32 Sensor Layer         │         ▼ FreeRTOS Tasks         │         ▼ Data Aggregation Layer         │         ├── HTTP Telemetry         │         └── MQTT Telemetry                 │                 ▼         Digital Twin & Dashboard                 │                 ▼       Predictive Maintenance 

---

## Digital Twin Concept

The Digital Twin will maintain a software representation of the CNC machine using live sensor data.

The goal is to continuously track:

- Machine operating status
- Vibration behaviour
- Temperature trends
- Pressure conditions
- Operator activity
- Fault events
- Maintenance indicators

This provides both real-time visibility and historical context for future analytics.

---

## Development Environment

Framework: ESP-IDF 5.5.4

Target: ESP32

Language: C

RTOS: FreeRTOS

---

## Building the Project

Navigate to the integrated firmware project:

bash cd integrated_rtos_work 

Build:

bash idf.py build 

Flash:

bash idf.py flash 

Monitor:

bash idf.py monitor 

---

## Future Direction

The next major milestone is establishing a complete telemetry pipeline:

text ESP32 Sensors       ↓ MQTT Broker       ↓ Digital Twin       ↓ Grafana Dashboard       ↓ Analytics & Predictive Maintenance 

Once the telemetry pipeline is fully operational, development will focus on:

- Machine health scoring
- FFT-based vibration analysis
- Anomaly detection
- Remaining Useful Life (RUL) estimation
- Predictive maintenance recommendations
- Dashboard visualizations and alerts

---

## Project Goal

The final system should continuously monitor a CNC machine, maintain a real-time digital representation of its state, identify abnormal operating conditions, and provide early warning signs of potential failures.

The aim is to reduce unplanned downtime, improve machine reliability, and move from reactive maintenance to predictive maintenance.
