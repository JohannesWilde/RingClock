# EpaperClock

Simple Clock using a DS3231 Real Time Clock [RTC] and a NeoPixel Ring [12x WS2812 5050 RGB LEDs].

Please note that for this to compile correctly one will have to adapt some files in one's Arduino installation. This is necessary as I wanted to run the Chip with the internal oscillator at 8MHz [please refer to [README_Fuses.md](README_Fuses.md) how to change F_CPU to 8000000 conveniently in the Arduino IDE] and employ a Meta Template Programming styled approach for changing the Pins [please refer to ArduinoDrivers/avrSrc/avrHeaderConverter.py].

Also I needed to bump up the supported C++ version to C++ 17 [see [ArduinoDrivers/README_C++.md](README_C++.md) for more details].
