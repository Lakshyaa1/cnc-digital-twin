# water_sensor — Digital Water Detection

Overview
- Simple digital water-detection example that reads a GPIO input and prints whether water is detected.

Interface & Pins (as used in `water_sensor/main/water_sensor.c`)
- Input: GPIO4 (digital input)

Notes
- This example treats the sensor module as a binary input. Some water sensors are analog; replace the module and logic accordingly.

Datasheet / Reference (example modules)
- KY-038 / generic water detection module overview: https://components101.com/sensors/water-level-sensor-module-ky-033
- Replace with your sensor's manufacturer datasheet for electrical details.
