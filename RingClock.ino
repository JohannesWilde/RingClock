/*
Clock display using an DS3231 RTC and a NeoPixel RGBW ring.
*/

#include "Colors.hpp"

#include <Adafruit_NeoPixel.h>
#include <DS3231.h>
#include <Wire.h>

namespace Pins {
int constexpr led = 3;
int constexpr powerRtc = A0;
}

uint16_t constexpr ledCount = 12;
uint8_t constexpr defaultMaxBrightness = 40;


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

  // Start the serial interface
  Serial.begin(57600);
}

uint8_t red = 255;
uint8_t green = 255;
uint8_t blue = 255;
uint8_t white = 255;

void loop() 
{
  // send what's going on to the serial monitor.

  // Get hour, minute, and second.
  bool h12Flag = false;
  bool pmFlag = false;
  Serial.print(myRTC.getHour(h12Flag, pmFlag), DEC);
  Serial.print(" ");
  uint8_t const minutes = myRTC.getMinute();
  Serial.print(minutes, DEC);
  Serial.print(" ");
  uint8_t const seconds = myRTC.getSecond();
  Serial.print(seconds, DEC);

  // Add AM/PM indicator
  if (h12Flag) {
    if (pmFlag) {
      Serial.print(" PM ");
    } else {
      Serial.print(" AM ");
    }
  } else {
    Serial.print(" 24h ");
  }

  // // Display the temperature
  // Serial.print("T=");
  // Serial.print(myRTC.getTemperature(), 2);
  
  strip.clear();

  uint16_t const pixelIndexSeconds = seconds % 12;
  strip.setPixelColor(pixelIndexSeconds, Colors::Red);
  
  uint16_t const pixelIndexMinutes = minutes % 12;
  strip.setPixelColor(pixelIndexMinutes, Colors::addColors(Colors::Green, strip.getPixelColor(pixelIndexMinutes)));
  
  // for(int i=0; i < strip.numPixels(); ++i)


  strip.show();                          //  Update strip to match

  // New line on display
  Serial.println();
  delay(1000);
}
