# Ultrasonic — HC-SR04-style Distance Sensor (RMT + GPIO)

Overview
- Example that triggers an ultrasonic ping and measures echo pulse width using the ESP32 RMT peripheral to compute distance in centimeters.

Interface & Pins (as used in `Ultrasonic/main/Ultrasonic.c`)
- TRIG: GPIO5
- ECHO: GPIO18
- Interface: GPIO + RMT peripheral for pulse timing

Notes
- The example uses `rmt_new_rx_channel` for echo capture and measures pulse width in microseconds.
- Ensure the trigger pin is driven with the correct 10 µs pulse as in `trigger_sensor()`.

Datasheet / Reference
- HC-SR04 typical datasheet: https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HC-SR04.pdf
