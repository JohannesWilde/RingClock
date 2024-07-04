Under Windows use WinAVR [install WinAVR and Git SCM [with MinTTY] and in MinTTY execute

PATH=/c/WinAVR/utils/bin:/c/WinAVR/bin:${PATH}

].

Under Linux use avrdude directly.

Setup the fuses for the Atmega328P as follows [0 - programmed, 1 - unprogrammed, big-endian Bit-order]:

[see [https://www.engbedded.com/fusecalc/](https://www.engbedded.com/fusecalc/)]

````{verbatim}
avrdude.exe -P COM3 -b 19200 -c avrisp -p atmega328p -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h

# 8MHz internal, no bootloader, no EEPROM-erase, no brown-out-detection
avrdude.exe -P COM3 -b 19200 -c avrisp -p atmega328p -U lfuse:w:0xE2:m -U hfuse:w:0xd7:m -U efuse:w:0x07:m

l: E2
h: d7
e: 07 [ff]
````



Arduino Uno defaults [16MHz, bootloader, EEPROM-erase, brown-out-detection 2.7V]:

````{verbatim}
avrdude.exe -P COM5 -b 19200 -c avrisp -p ATtiny85 -U lfuse:w:0xff:m -U hfuse:w:0xde:m -U efuse:w:0x05:m

l: ff
h: de
e: 05 [fd]
````








I added the following to this file:

c:\Users\User\AppData\Local\Arduino15\packages\arduino\hardware\avr\1.8.6\boards.txt

This lets one compile code for an atmega328p at 8MHz. Please note however, that the programming of the fuses has to be done manually.


````{verbatim}
##############################################################
################ custom addition by J.W. #####################

unoEightMhz.name=Arduino Uno (8MHz)

unoEightMhz.vid.0=0x2341
unoEightMhz.pid.0=0x0043
unoEightMhz.vid.1=0x2341
unoEightMhz.pid.1=0x0001
unoEightMhz.vid.2=0x2A03
unoEightMhz.pid.2=0x0043
unoEightMhz.vid.3=0x2341
unoEightMhz.pid.3=0x0243

unoEightMhz.upload.tool=avrdude
unoEightMhz.upload.tool.default=avrdude
unoEightMhz.upload.tool.network=arduino_ota
unoEightMhz.upload.protocol=arduino
unoEightMhz.upload.maximum_size=32256
unoEightMhz.upload.maximum_data_size=2048
unoEightMhz.upload.speed=115200

# keep the original fuses [16MHz], as to not unintendetly render the programming arduino unprogrammable [the bootloader here expects 16MHz!].
# unoEightMhz.bootloader.tool=avrdude
# unoEightMhz.bootloader.low_fuses=0xE2
# unoEightMhz.bootloader.high_fuses=0xD7
# unoEightMhz.bootloader.extended_fuses=0x07
# unoEightMhz.bootloader.unlock_bits=0x3F
# unoEightMhz.bootloader.lock_bits=0x0F
# unoEightMhz.bootloader.file="no bootloader on this configuration"

unoEightMhz.build.mcu=atmega328p
unoEightMhz.build.f_cpu=8000000L
unoEightMhz.build.board=AVR_UNO
unoEightMhz.build.core=arduino
unoEightMhz.build.variant=standard

################ custom addition by J.W. #####################
##############################################################
````
