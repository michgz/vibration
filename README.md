# Vibration stimulation and response analyser

Produces a mechanical vibration stimulus and records the response at a nearby point on the structure, implemented on the powerful and low-cost ESP32 hardware. It's intended for modal analysis of built structures (for the purpose of designing vibration suppression mechanisms), and is WiFi-enabled to make it as flexible as possible in its use.

Features of version 1.0:
- Controlled by a HTML interface, exposed through a simple HTTP 1.1 server and communicating over WiFi
- Frequency range 1-60 Hz
- Adjustable amplitude
- Dual-polarity PWM output, drives a voice coil through a L298 driver or similar
- Supports digital accelerometers via I2C. Two options:
  - LSM6DS3: Price ~$1 US wholesale, and readily available on Ebay in breakout board form. Noise performance: 90 ug/rtHz (very good for the price!)
  - ADXL355: Expensive (>$35) but superb noise performance: 25 ug/rtHz. Best source at the moment seems to be direct from Analog.com, hopefully will become more mainstream soon.

Features of future versions:
- Support for analogue accelerometers, particularly ADXL354 and piezoelectrics transducers
- Ethernet communications.

# Reference Hardware

In this project I've been using breakout boards for each component, breadboarded together. If using the ADXL355 accelerometer it would be possible to create a custom PCB for the whole circuit - if anyone has a suitable design they wish to share, please get in touch with the project. The LSM6DS3 accelerometer is a bit more tricky for a hobbyist to solder, I recommend using a breakout board for that one.

Components:
- ESP32-DEVKITC, which combines the WROOM-32 module (rev. 1 silicon) with a 5V-to-3.3V regulator and some other circuitry
- EVAL-ADXL355Z breakout board for the accelerometer (requires 3.3V power)
- L298N dual H-bridge module with 12V-to-5V regulator, such as https://www.robotshop.com/uk/dc-motor-driver-module.html. I got a cheap one off e-bay, but unfortunately the regulator doesn't seem to be powerful enough to power the ESP32. Still investigating what's best here.
- Voice coil. I'm using an old car audio speaker, but for better power and actuation movement consider a high-quality unit from http://moticont.com/ or http://www.beikimco.com/. I also use a 100R 1W resistor in series with the voice coil; it is _strongly recommended_ to have a resistive limiter of this sort, to avoid too much power going into the mechanical system. Don't rely on the software to do the right thing!!
- 12V DC supply
- 5V USB supply (if your L298N board isn't suitable for supplying 5V, as in my case).

# Software Dependencies

- ESP-IDF v3.0
