/*
Clock display using an DS3231 RTC and a NeoPixel RGBW ring.
*/

#include "Colors.hpp"
#include "NeoPixelPatterns.hpp"

#include "ArduinoDrivers/ArduinoUno.hpp"
#include "ArduinoDrivers/avrpinspecializations.hpp"
#include "ArduinoDrivers/button.hpp"
#include "ArduinoDrivers/buttonTimed.hpp"
#include "ArduinoDrivers/simplePinAvr.hpp"

#include "helpers/tmpLoop.hpp"

#include <Adafruit_NeoPixel.h>
#include <DS3231.h>
#include <Wire.h>


// Classes, structs and methods.

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

static TimeOfDay getTimeOfDayFromRTC(DS3231 & rtc)
{
    // Get hour, minute, and second.
    bool h12Flag = false;
    bool pmFlag = false;
    TimeOfDay const timeOfDay(rtc.getHour(h12Flag, pmFlag),
                              rtc.getMinute(),
                              rtc.getSecond());

    return timeOfDay;
}

static void showTimeOfDay(Adafruit_NeoPixel & strip, TimeOfDay const & timeOfDay, double subseconds = 0.)
{
    double const secondsAndSubseconds = static_cast<double>(timeOfDay.seconds) + subseconds;

    strip.clear();

    uint16_t const pixelIndexHours = (timeOfDay.hours % 12) * strip.numPixels() / 12;
    strip.setPixelColor(pixelIndexHours, Colors::addColors(Colors::Blue, strip.getPixelColor(pixelIndexHours)));

    double const pixelIndexMinutes = (static_cast<double>(timeOfDay.minutes) + (secondsAndSubseconds / 60.)) / 60. * strip.numPixels();
    NeoPixelPatterns::addColorsWrapping(strip,
                                        pixelIndexMinutes,
                                        NeoPixelPatterns::brightnessFunctionMountain,
                                        Colors::Green);

    double const pixelIndexSeconds = secondsAndSubseconds / 60. * strip.numPixels();
    NeoPixelPatterns::addColorsWrapping(strip,
                                        pixelIndexSeconds,
                                        NeoPixelPatterns::brightnessFunctionMountain,
                                        Colors::Red);

    strip.show();                          //  Update strip to match
}

static void serialPrintTimeOfDay(TimeOfDay const & timeOfDay)
{
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
}

template<class ButtonTimed_>
static void serialPrintButton(char const * const name)
{
    char const * state = nullptr;

    if (ButtonTimed_::isDownLong())
    {
        state = "downLong";
    }
    else if (ButtonTimed_::isDownShort())
    {
        state = "downShort";
    }
    else if (ButtonTimed_::isDown())
    {
        state = "down";
    }

    if (nullptr != state)
    {
        Serial.print(name);
        Serial.print(": ");
        Serial.print(state);
        Serial.println();
    }
}

enum class OperationalMode
{
    Clock,
    Settings
};


// Static variables and instances.

namespace Pins
{
int constexpr led = 3;
}

uint16_t constexpr ledCount = 12;
uint8_t constexpr defaultMaxBrightness = 200;

#define PRINT_SERIAL_TIME false
#define PRINT_SERIAL_BUTTONS true

DS3231 myRTC;

// Declare our NeoPixel strip object:
// Adafruit_NeoPixel strip(ledCount, Pins::led, NEO_GRBW + NEO_KHZ800); // testing strip
Adafruit_NeoPixel strip(ledCount, Pins::led, NEO_GRB + NEO_KHZ800); // 12-LEDs ring
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)


uint8_t constexpr cycleDurationMs = 50;
uint8_t constexpr shortPressCount = 2;
uint8_t constexpr longPressCount = 10;

typedef ButtonTimed<Button<SimplePinAvrRead<ArduinoUno::D11, AvrInputOutput::InputPullup>, SimplePin::State::Zero>, shortPressCount, longPressCount> ButtonTop;
typedef ButtonTimed<Button<SimplePinAvrRead<ArduinoUno::D8, AvrInputOutput::InputPullup>, SimplePin::State::Zero>, shortPressCount, longPressCount> ButtonRight;
typedef ButtonTimed<Button<SimplePinAvrRead<ArduinoUno::D9, AvrInputOutput::InputPullup>, SimplePin::State::Zero>, shortPressCount, longPressCount> ButtonBottom;
typedef ButtonTimed<Button<SimplePinAvrRead<ArduinoUno::D10, AvrInputOutput::InputPullup>, SimplePin::State::Zero>, shortPressCount, longPressCount> ButtonLeft;


template <uint8_t index>
class Buttons;

template <> class Buttons<0> : public ButtonTop {/* intentionally empty */};
template <> class Buttons<1> : public ButtonRight {/* intentionally empty */};
template <> class Buttons<2> : public ButtonBottom {/* intentionally empty */};
template <> class Buttons<3> : public ButtonLeft {/* intentionally empty */};

typedef AvrPinOutput<ArduinoUno::A0::Register, ArduinoUno::A0::pinNumber> PinPowerRtc;


// Wrappers for loops.
template<uint8_t Index>
struct WrapperInitialize
{
    static void impl()
    {
        Buttons<Index>::initialize();
    }
};

template<uint8_t Index>
struct WrapperUpdate
{
    static void impl()
    {
        Buttons<Index>::update();
    }
};


OperationalMode operationalMode = OperationalMode::Clock;

// setup() and loop() functionality.

void setup()
{
    // Power for the RTC, as I don't have enough 5V ports on the UNO.
    PinPowerRtc::initialize<AvrInputOutput::PinState::High>();

    Helpers::TMP::Loop<4, WrapperInitialize>::impl();

    // Start the I2C interface
    Wire.begin();

    // Use 12h-Mode.
    myRTC.setClockMode(true);

    // Startup LEDs
    strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
    strip.show();            // Turn OFF all pixels ASAP
    strip.setBrightness(defaultMaxBrightness);

#if PRINT_SERIAL_TIME || PRINT_SERIAL_BUTTONS
    // Start the serial interface
    Serial.begin(57600);
#endif
}


namespace Common
{

bool modeChangeButtonReleasedOnceInThisMode = true;

} // namespace Common

namespace Clock
{
// typedef ButtonTop;
typedef ButtonRight ButtonSettings;
// typedef ButtonBottom;
// typedef ButtonLeft;

static uint8_t constexpr previousSecondsInvalid = 255;

static uint8_t previousSeconds = previousSecondsInvalid;
static unsigned long lastSecondChangeTime = 0;
} // namespace Clock


namespace Settings
{
typedef ButtonTop ButtonUp;
typedef ButtonRight ButtonExit;
typedef ButtonBottom ButtonDown;
// typedef ButtonLeft;

TimeOfDay timeOfDay;
} // namespace Settings

void loop()
{
    Helpers::TMP::Loop<4, WrapperUpdate>::impl();

#if PRINT_SERIAL_BUTTONS
    serialPrintButton<ButtonTop>("ButtonTop");
    serialPrintButton<ButtonRight>("ButtonRight");
    serialPrintButton<ButtonBottom>("ButtonBottom");
    serialPrintButton<ButtonLeft>("ButtonLeft");
#endif

    switch (operationalMode)
    {
    case OperationalMode::Clock:
    {
        bool displayTime = true;

        if (!Common::modeChangeButtonReleasedOnceInThisMode)
        {
            if (Clock::ButtonSettings::releasedAfterLong())
            {
                Common::modeChangeButtonReleasedOnceInThisMode = true;
            }
        }
        else if (Clock::ButtonSettings::isDownLong())
        {
            operationalMode = OperationalMode::Settings;
            Settings::timeOfDay = getTimeOfDayFromRTC(myRTC);
            Clock::previousSeconds = Clock::previousSecondsInvalid;
            displayTime = false;

            Common::modeChangeButtonReleasedOnceInThisMode = false;
        }

        if (displayTime)
        {
            // Get hour, minutes, and seconds from RTC.
            TimeOfDay const timeOfDay = getTimeOfDayFromRTC(myRTC);

            // simulate subseconds
            double subseconds = 0.;
            if (Clock::previousSeconds != timeOfDay.seconds)
            {
                // seconds changed -> reset subseconds to 0
                Clock::lastSecondChangeTime = millis();
                Clock::previousSeconds = timeOfDay.seconds;
            }
            else
            {
                // simulate via millis()
                subseconds = static_cast<double>(millis() - Clock::lastSecondChangeTime) / 1000.;
            }

            // Create color representation.
            showTimeOfDay(strip, timeOfDay, subseconds);


#if PRINT_SERIAL_TIME
            serialPrintTimeOfDay(timeOfDay);
#endif
        }

        break;
    }
    case OperationalMode::Settings:
    {
        bool displayTime = true;

        if (!Common::modeChangeButtonReleasedOnceInThisMode)
        {
            if (Clock::ButtonSettings::releasedAfterLong())
            {
                Common::modeChangeButtonReleasedOnceInThisMode = true;
            }
        }
        else if (Clock::ButtonSettings::isDownLong())
        {
            operationalMode = OperationalMode::Clock;

            myRTC.setSecond(Settings::timeOfDay.seconds);
            myRTC.setMinute(Settings::timeOfDay.minutes);
            myRTC.setHour(Settings::timeOfDay.hours);
            // myRTC.setDoW(7);
            // myRTC.setDate(9);
            // myRTC.setMonth(6);
            // myRTC.setYear(24);
#if PRINT_SERIAL_TIME
            Serial.print("Settings: ");
            serialPrintTimeOfDay(Settings::timeOfDay);
#endif

            displayTime = false;

            Common::modeChangeButtonReleasedOnceInThisMode = false;
        }

        if (displayTime)
        {
            // Create color representation.
            showTimeOfDay(strip, Settings::timeOfDay);
        }
    }
    }

    delay(cycleDurationMs);
}
