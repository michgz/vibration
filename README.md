# vibration
Vibration stimulation and response analyser

Produces a mechanical vibration stimulus and records the response at a nearby point on the same structure. It's intended for modal analysis of built structures (for the purpose of designing vibration control mechanisms), and is WiFi-enabled to make it flexible in its use.

Features of version 1.0:
- Controlled by a HTTP interface
- Frequency range 1-60 Hz
- Dual-polarity PWM output, drives a voice coil through a L298 driver or similar
- Supports digital accelerometers via I2C. Two options:
  - LSM6: cheap and readily available. Noise performance: 90 ug/rtHz (very good for the price!)
  - ADXL355: expensive but superb noise performance: 25 ug/rtHz.
- ESP32 hardware
