/*
Clock display using an DS3231 RTC and a NeoPixel RGBW ring.
*/

#include "Colors.hpp"
#include "NeoPixelPatterns.hpp"

#include <Adafruit_NeoPixel.h>
#include <DS3231.h>
#include <Wire.h>

struct TimeOfDay
{
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;

    TimeOfDay(uint8_t const hours = 0,
              uint8_t const minutes = 0,
              uint8_t const seconds = 0)
        : hours(hours)
        , minutes(minutes)
        , seconds(seconds)
    {
        // intentinoally empty
    }
    TimeOfDay(const TimeOfDay &) = default;
    TimeOfDay(TimeOfDay &&) = default;
    TimeOfDay &operator=(const TimeOfDay &) = default;
    TimeOfDay &operator=(TimeOfDay &&) = default;
};

namespace Pins
{
int constexpr led = 3;
int constexpr powerRtc = A0;
}

uint16_t constexpr ledCount = 12;
uint8_t constexpr defaultMaxBrightness = 200;

#define PRINT_SERIAL_OUTPUT false

DS3231 myRTC;

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(ledCount, Pins::led, NEO_GRBW + NEO_KHZ800);
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)

void setup() {
    // Power for the RTC, as I don't have enough 5V ports on the UNO.
    pinMode(Pins::powerRtc, OUTPUT);
    digitalWrite(Pins::powerRtc, HIGH);

    // Start the I2C interface
    Wire.begin();

    // Use 12h-Mode.
    myRTC.setClockMode(true);

    // Startup LEDs
    strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
    strip.show();            // Turn OFF all pixels ASAP
    strip.setBrightness(defaultMaxBrightness);

// myRTC.setSecond(0);
// myRTC.setMinute(32);
// myRTC.setHour(17);
// myRTC.setDoW(7);
// myRTC.setDate(9);
// myRTC.setMonth(6);
// myRTC.setYear(24);

#if PRINT_SERIAL_OUTPUT
    // Start the serial interface
    Serial.begin(57600);
#endif
}


static uint8_t previousSeconds = 255; // invalid
static unsigned long lastSecondChangeTime = 0;

void loop() 
{
    // Get hour, minute, and second.
    bool h12Flag = false;
    bool pmFlag = false;
    TimeOfDay const timeOfDay(myRTC.getHour(h12Flag, pmFlag),
                              myRTC.getMinute(),
                              myRTC.getSecond());

    // simulate subseconds
    double secondsAndSubseconds = static_cast<double>(timeOfDay.seconds);
    if (previousSeconds != timeOfDay.seconds)
    {
        // seconds changed -> reset subseconds to 0
        lastSecondChangeTime = millis();
        previousSeconds = timeOfDay.seconds;
    }
    else
    {
        // simulate via millis()
        secondsAndSubseconds += static_cast<double>(millis() - lastSecondChangeTime) / 1000.;
    }

    // Create color representation.
    strip.clear();

    uint16_t const pixelIndexHours = timeOfDay.hours % 12;
    strip.setPixelColor(pixelIndexHours, Colors::addColors(Colors::Blue, strip.getPixelColor(pixelIndexHours)));

    double const pixelIndexMinutes = (static_cast<double>(timeOfDay.minutes) + (secondsAndSubseconds / 60.)) / 60. * ledCount;
    NeoPixelPatterns::addColorsWrapping(strip,
                                        pixelIndexMinutes,
                                        NeoPixelPatterns::brightnessFunctionMountain,
                                        Colors::Green);

    double const pixelIndexSeconds = secondsAndSubseconds / 60. * ledCount;
    NeoPixelPatterns::addColorsWrapping(strip,
                                        pixelIndexSeconds,
                                        NeoPixelPatterns::brightnessFunctionMountain,
                                        Colors::Red);

    strip.show();                          //  Update strip to match


#if PRINT_SERIAL_OUTPUT
    // send what's going on to the serial monitor.
    Serial.print(timeOfDay.hours, DEC);
    Serial.print(":");
    Serial.print(timeOfDay.minutes, DEC);
    Serial.print(":");
    Serial.print(timeOfDay.seconds, DEC);

    // // Add AM/PM indicator
    // if (h12Flag) {
    //   if (pmFlag) {
    //     Serial.print(" PM ");
    //   } else {
    //     Serial.print(" AM ");
    //   }
    // } else {
    //   Serial.print(" 24h ");
    // }

    // // Display the temperature
    // Serial.print("T=");
    // Serial.print(myRTC.getTemperature(), 2);

    Serial.println();
#endif

    delay(50);
}
