# vibration
Vibration stimulation and response analyser

Produces a mechanical vibration stimulus and records the response at a nearby point on the same structure. It's intended for modal analysis of built structures (for the purpose of designing vibration control mechanisms), and is WiFi-enabled to make it flexible in its use.

Features of version 1.0:
- Controlled by a HTML interface
- Frequency range 1-60 Hz
- Adjustable amplitude
- Dual-polarity PWM output, drives a voice coil through a L298 driver or similar
- Supports digital accelerometers via I2C. Two options:
  - LSM6DS3: Price ~$1 US wholesale, and readily available on Ebay in breakout board form. Noise performance: 90 ug/rtHz (very good for the price!)
  - ADXL355: Expensive (>$35!) but superb noise performance: 25 ug/rtHz. Best source at the moment seems to be direct from Analog.com, hopefully will become more mainstream soon.
- ESP32 hardware

Would like in future to support analogue accelerometers, particularly ADXL354, and piezoelectrics transducers.
