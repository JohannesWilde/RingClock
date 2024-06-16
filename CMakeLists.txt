# avoid compile check for cross-compile targets
# https://stackoverflow.com/questions/53633705/cmake-the-c-compiler-is-not-able-to-compile-a-simple-test-program
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

cmake_minimum_required(VERSION 3.19)

# This is a dummy project, as to have convenient code highlighting in the QtCreator IDE.
# This won't actually compile with it as of now.
# For compilation use the Arduino IDE [necessary adaptations are referenced in README.md].
project(EpaperClockDummy)


# For correct highlighting in QtCreator check Preferences->Environment->MIME Types->text/x-c++src to include "*.ino" in Patterns.
add_executable(${PROJECT_NAME}
    Colors.cpp
    NeoPixelPatterns.cpp
    RingClock.cpp
)

# For configurability the following two environment variables are now required to be defined:
#   - ARDUINO_INSTALLATION_DIRECTORY        [e.g. C:\Users\...\AppData\Local\Arduino15\packages\arduino]
#   - ARDUINO_USER_LIBRARIES_DIRECTORY      [e.g. C:\Users\...\Documents\Arduino\libraries]
target_include_directories(${PROJECT_NAME}
    PRIVATE "$ENV{ARDUINO_INSTALLATION_DIRECTORY}/hardware/avr/1.8.6/cores/arduino"
    PRIVATE "$ENV{ARDUINO_INSTALLATION_DIRECTORY}/hardware/avr/1.8.6/variants/standard"
    PRIVATE "$ENV{ARDUINO_INSTALLATION_DIRECTORY}/hardware/avr/1.8.6/libraries/Wire/src"
    PRIVATE "$ENV{ARDUINO_USER_LIBRARIES_DIRECTORY}/Adafruit_NeoPixel"
    PRIVATE "$ENV{ARDUINO_USER_LIBRARIES_DIRECTORY}/DS3231"
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE __AVR_ATmega328P__
    PRIVATE F_CPU=1000000
)

target_link_libraries(EpaperClockDummy
    PRIVATE arduinoDrivers
    PRIVATE helpers
)

add_subdirectory(ArduinoDrivers)
add_subdirectory(helpers)
