# new_pressure — Analog Pressure Sensor (ADC)

Overview
- Example that reads an analog pressure sensor via the ADC and converts voltage to pressure using a simple linear calibration.

Interface & Pins (as used in `new_pressure/main/new_pressure.c`)
- Interface: ADC (ADC1 oneshot)
- ADC Channel: ADC_CHANNEL_6 (GPIO34)

Notes
- The example uses a resistor divider (R_TOP=5600Ω, R_BOTTOM=15000Ω) and example characterization values. Replace these with values from your sensor's datasheet for accurate conversion.

Datasheet (example references)
- If you have a specific analog pressure transducer, attach its manufacturer datasheet. Example analog sensor docs:
  - Honeywell TruStability/SSC series (example): https://sps.honeywell.com/content/dam/sps/products/pressure/sscdatasheet.pdf
  - MPX-series from NXP (example): https://www.nxp.com/docs/en/data-sheet/MPXHZ1.pdf
