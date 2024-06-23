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

enum class SelectableColor
{
    blue,
    green,
    red,
    yellow,
    white
};

SelectableColor nextSelectableColor(SelectableColor const selectableColor)
{
    SelectableColor color = SelectableColor::white;
    switch (selectableColor)
    {
    case SelectableColor::blue:
    {
        color = SelectableColor::green;
        break;
    }
    case SelectableColor::green:
    {
        color = SelectableColor::red;
        break;
    }
    case SelectableColor::red:
    {
        color = SelectableColor::yellow;
        break;
    }
    case SelectableColor::yellow:
    {
        color = SelectableColor::white;
        break;
    }
    case SelectableColor::white:
    {
        color = SelectableColor::blue;
        break;
    }
    }
    return color;
}

SelectableColor previousSelectableColor(SelectableColor const selectableColor)
{
    SelectableColor color = SelectableColor::white;
    switch (selectableColor)
    {
    case SelectableColor::blue:
    {
        color = SelectableColor::white;
        break;
    }
    case SelectableColor::green:
    {
        color = SelectableColor::blue;
        break;
    }
    case SelectableColor::red:
    {
        color = SelectableColor::green;
        break;
    }
    case SelectableColor::yellow:
    {
        color = SelectableColor::red;
        break;
    }
    case SelectableColor::white:
    {
        color = SelectableColor::yellow;
        break;
    }
    }
    return color;
}

Colors::Color_t colorFrom(SelectableColor const selectableColor)
{
    Colors::Color_t color = Colors::Black;
    switch (selectableColor)
    {
    case SelectableColor::blue:
    {
        color = Colors::Blue;
        break;
    }
    case SelectableColor::green:
    {
        color = Colors::Green;
        break;
    }
    case SelectableColor::red:
    {
        color = Colors::Red;
        break;
    }
    case SelectableColor::yellow:
    {
        color = Colors::Yellow;
        break;
    }
    case SelectableColor::white:
    {
        color = Colors::White;
        break;
    }
    }
    return color;
}


enum class DisplayComponent
{
    hours,
    minutes,
    seconds
};

struct ColorSettings
{
    SelectableColor selectableColor = SelectableColor::white;
    uint8_t brightness = 255;

    Colors::Color_t scaledColor() const
    {
        return Colors::colorScaleBrightness(colorFrom(selectableColor), static_cast<double>(brightness) / 255.);
    }
};

struct ColorsSettings
{
    ColorSettings colorsSettings[3];

    ColorSettings & at(DisplayComponent const component)
    {
        ColorSettings * colorSettings = nullptr;
        switch (component)
        {
        case DisplayComponent::hours:
        {
            colorSettings = &colorsSettings[0];
            break;
        }
        case DisplayComponent::minutes:
        {
            colorSettings = &colorsSettings[1];
            break;
        }
        case DisplayComponent::seconds:
        {
            colorSettings = &colorsSettings[2];
            break;
        }
        }
        return *colorSettings;
    }

    ColorSettings const & at(DisplayComponent const component) const
    {
        ColorSettings const * colorSettings = nullptr;
        switch (component)
        {
        case DisplayComponent::hours:
        {
            colorSettings = &colorsSettings[0];
            break;
        }
        case DisplayComponent::minutes:
        {
            colorSettings = &colorsSettings[1];
            break;
        }
        case DisplayComponent::seconds:
        {
            colorSettings = &colorsSettings[2];
            break;
        }
        }
        return *colorSettings;
    }

    bool colorConflicts(SelectableColor const color) const
    {
        return ((color == at(DisplayComponent::hours).selectableColor)
                || (color == at(DisplayComponent::minutes).selectableColor)
                || (color == at(DisplayComponent::seconds).selectableColor));
    }
};


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

static void showTimeOfDay(Adafruit_NeoPixel & strip, TimeOfDay const & timeOfDay, double subseconds, ColorsSettings const & colorsSettings)
{
    double const secondsAndSubseconds = static_cast<double>(timeOfDay.seconds) + subseconds;

    strip.clear();

    uint16_t const pixelIndexHours = (timeOfDay.hours % 12) * strip.numPixels() / 12;
    strip.setPixelColor(pixelIndexHours, Colors::addColors(colorsSettings.at(DisplayComponent::hours).scaledColor(),
                                                           strip.getPixelColor(pixelIndexHours)));

    double const pixelIndexMinutes = (static_cast<double>(timeOfDay.minutes) + (secondsAndSubseconds / 60.)) / 60. * strip.numPixels();
    NeoPixelPatterns::addColorsWrapping(strip,
                                        pixelIndexMinutes,
                                        NeoPixelPatterns::brightnessFunctionMountain,
                                        colorsSettings.at(DisplayComponent::minutes).scaledColor());

    double const pixelIndexSeconds = secondsAndSubseconds / 60. * strip.numPixels();
    NeoPixelPatterns::addColorsWrapping(strip,
                                        pixelIndexSeconds,
                                        NeoPixelPatterns::brightnessFunctionMountain,
                                        colorsSettings.at(DisplayComponent::seconds).scaledColor());

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
    clock,
    settings
};

enum class SettingsSelection
{
    hours,
    minutes,
    seconds
};

DisplayComponent displayComponentFrom(SettingsSelection const selection)
{
    DisplayComponent component = DisplayComponent::hours;
    switch (selection)
    {
    case SettingsSelection::hours:
    {
        component = DisplayComponent::hours;
        break;
    }
    case SettingsSelection::minutes:
    {
        component = DisplayComponent::minutes;
        break;
    }
    case SettingsSelection::seconds:
    {
        component = DisplayComponent::seconds;
        break;
    }
    }
    return component;
}

enum class SettingsType
{
    value,
    brightness,
    color
};

SettingsSelection nextSettingsSelection(SettingsSelection const selection)
{
    SettingsSelection newSelection = SettingsSelection::seconds;
    switch (selection)
    {
    case SettingsSelection::hours:
    {
        newSelection = SettingsSelection::minutes;
        break;
    }
    case SettingsSelection::minutes:
    {
        newSelection = SettingsSelection::seconds;
        break;
    }
    case SettingsSelection::seconds:
    {
        newSelection = SettingsSelection::hours;
        break;
    }
    }
    return newSelection;
}

uint8_t * timeOfDayComponentForSelection(TimeOfDay & timeOfDay, SettingsSelection const selection)
{
    uint8_t * newSelection = nullptr;
    switch (selection)
    {
    case SettingsSelection::hours:
    {
        newSelection = &timeOfDay.hours;
        break;
    }
    case SettingsSelection::minutes:
    {
        newSelection = &timeOfDay.minutes;
        break;
    }
    case SettingsSelection::seconds:
    {
        newSelection = &timeOfDay.seconds;
        break;
    }
    }
    return newSelection;
}

uint8_t timeOfDayComponentWrapAroundForSelection(SettingsSelection const selection)
{
    uint8_t wrapAround = 255;
    switch (selection)
    {
    case SettingsSelection::hours:
    {
        wrapAround = 12;
        break;
    }
    case SettingsSelection::minutes:
        [[fallthrough]];
    case SettingsSelection::seconds:
    {
        wrapAround = 60;
        break;
    }
    }
    return wrapAround;
}


uint8_t incrementUint8Capped(uint8_t const value)
{
    uint8_t newValue = value;
    if (newValue < UCHAR_MAX)
    {
        ++newValue;
    }
    return newValue;
}

bool longPressDurationActive(uint8_t const longPressDuration)
{
    // ramp up selection speed by decreasing the difference between true-returns.
    switch (longPressDuration)
    {
    case 0:
        // fall through
    case 5:
        // fall through
    case 10:
        // fall through
    case 14:
        // fall through
    case 18:
        // fall through
    case 21:
        // fall through
    case 24:
        // fall through
    case 27:
        // fall through
    case 29:
        // fall through
    case 31:
        // fall through
    case 33:
        // fall through
    case 35:
    {
        return true;
    }
    default:
    {
        return (37 <= longPressDuration);
    }
    }
}

static void incrementTimeOfDayComponent(TimeOfDay & timeOfDay, SettingsSelection const settingsSelection)
{
    uint8_t * const component = timeOfDayComponentForSelection(timeOfDay, settingsSelection);
    uint8_t const wrapAround = timeOfDayComponentWrapAroundForSelection(settingsSelection);
    (*component) += 1;
    while (wrapAround <= (*component))
    {
        (*component) -= wrapAround;
    }
}

static void decrementTimeOfDayComponent(TimeOfDay & timeOfDay, SettingsSelection const settingsSelection)
{
    uint8_t * const component = timeOfDayComponentForSelection(timeOfDay, settingsSelection);
    uint8_t const wrapAround = timeOfDayComponentWrapAroundForSelection(settingsSelection);
    if (0 == (*component))
    {
        (*component) = wrapAround;
    }
    (*component) -= 1;
}


// Static variables and instances.

namespace Pins
{
int constexpr led = 3;
}

uint16_t constexpr ledCount = 12;
uint8_t constexpr defaultMaxBrightness = 200;

#define PRINT_SERIAL_TIME true
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


OperationalMode operationalMode = OperationalMode::clock;
ColorsSettings colorsSettings;

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
    strip.setBrightness(255);

    // Todo: save and load settings from eeprom
    colorsSettings.at(DisplayComponent::hours).brightness = defaultMaxBrightness;
    colorsSettings.at(DisplayComponent::hours).selectableColor = SelectableColor::blue;
    colorsSettings.at(DisplayComponent::minutes).brightness = defaultMaxBrightness;
    colorsSettings.at(DisplayComponent::minutes).selectableColor = SelectableColor::green;
    colorsSettings.at(DisplayComponent::seconds).brightness = defaultMaxBrightness;
    colorsSettings.at(DisplayComponent::seconds).selectableColor = SelectableColor::red;

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
typedef ButtonRight ButtonSelectOrExit;
typedef ButtonBottom ButtonDown;
typedef ButtonLeft ButtonBrightnessOrColor;

TimeOfDay timeOfDay;
SettingsSelection settingsSelection = SettingsSelection::hours;
SettingsType settingsType = SettingsType::value;
} // namespace settings

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
    case OperationalMode::clock:
    {
        bool displayTime = true;

        if (!Common::modeChangeButtonReleasedOnceInThisMode)
        {
            if (Settings::ButtonSelectOrExit::releasedAfterLong())
            {
                Common::modeChangeButtonReleasedOnceInThisMode = true;
            }
        }
        else if (Clock::ButtonSettings::isDownLong())
        {
            operationalMode = OperationalMode::settings;
            Settings::timeOfDay = getTimeOfDayFromRTC(myRTC);
            Settings::settingsSelection = SettingsSelection::hours;
            SettingsType settingsType = SettingsType::value;

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
            showTimeOfDay(strip, timeOfDay, subseconds, colorsSettings);


#if PRINT_SERIAL_TIME
            serialPrintTimeOfDay(timeOfDay);
#endif
        }

        break;
    }
    case OperationalMode::settings:
    {
        static uint8_t longPressDurationAccumulation = 0;

        bool displayTime = true;

        if (Settings::ButtonBrightnessOrColor::isDownLong())
        {
            Settings::settingsType = SettingsType::brightness;
        }
        else
        {
            // do nothing
        }

        if (!Common::modeChangeButtonReleasedOnceInThisMode)
        {
            if (Clock::ButtonSettings::releasedAfterLong())
            {
                Common::modeChangeButtonReleasedOnceInThisMode = true;
                longPressDurationAccumulation = 0;
                Settings::settingsType = SettingsType::value;
            }
        }
        else if (Settings::ButtonSelectOrExit::isDownLong())
        {
            // todo: save colorSettings to eeprom

            operationalMode = OperationalMode::clock;

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
        else if (Settings::ButtonSelectOrExit::pressed())
        {
            Settings::settingsSelection = nextSettingsSelection(Settings::settingsSelection);
            longPressDurationAccumulation = 0;
        }
        else if (Settings::ButtonUp::isDown() && Settings::ButtonDown::isDown())
        {
            // pressing both resets rampup
            longPressDurationAccumulation = 0;
        }
        else if (Settings::ButtonBrightnessOrColor::isDownShort())
        {
            // reset longPressDuration once this button is pressed
            longPressDurationAccumulation = 0;
        }
        else if (Settings::ButtonBrightnessOrColor::releasedAfterShort())
        {
            switch (Settings::settingsType)
            {
            case SettingsType::value:
            {
                Settings::settingsType = SettingsType::color;
                break;
            }
            case SettingsType::brightness:
            {
                // this code should be unreachable
                break;
            }
            case SettingsType::color:
            {
                Settings::settingsType = SettingsType::value;
                break;
            }
            };
        }
        else if (Settings::ButtonBrightnessOrColor::releasedAfterLong())
        {
            Settings::settingsType = SettingsType::value;
            longPressDurationAccumulation = 0;
        }
        else if (Settings::ButtonUp::isDownLong())
        {
            longPressDurationAccumulation = incrementUint8Capped(longPressDurationAccumulation);
            if (longPressDurationActive(longPressDurationAccumulation))
            {
                switch (Settings::settingsType)
                {
                case SettingsType::value:
                {
                    incrementTimeOfDayComponent(Settings::timeOfDay, Settings::settingsSelection);
                    break;
                }
                case SettingsType::brightness:
                {
                    // allow uint8_t overflow
                    colorsSettings.at(displayComponentFrom(Settings::settingsSelection)).brightness += 2;
                    break;
                }
                case SettingsType::color:
                {
                    SelectableColor & colorToModify = colorsSettings.at(displayComponentFrom(Settings::settingsSelection)).selectableColor;
                    SelectableColor newColor = nextSelectableColor(colorToModify);
                    while (colorsSettings.colorConflicts(newColor))
                    {
                        newColor = nextSelectableColor(newColor);
                    }
                    colorToModify = newColor;
                    break;
                }
                }
            }
        }
        else if (Settings::ButtonDown::isDownLong())
        {
            longPressDurationAccumulation = incrementUint8Capped(longPressDurationAccumulation);
            if (longPressDurationActive(longPressDurationAccumulation))
            {
                switch (Settings::settingsType)
                {
                case SettingsType::value:
                {
                    decrementTimeOfDayComponent(Settings::timeOfDay, Settings::settingsSelection);
                    break;
                }
                case SettingsType::brightness:
                {
                    // allow uint8_t overflow
                    colorsSettings.at(displayComponentFrom(Settings::settingsSelection)).brightness -= 2;
                    break;
                }
                case SettingsType::color:
                {
                    SelectableColor & colorToModify = colorsSettings.at(displayComponentFrom(Settings::settingsSelection)).selectableColor;
                    SelectableColor newColor = previousSelectableColor(colorToModify);
                    while (colorsSettings.colorConflicts(newColor))
                    {
                        newColor = previousSelectableColor(newColor);
                    }
                    colorToModify = newColor;
                    break;
                }
                }
            }
        }
        else if (Settings::ButtonUp::releasedAfterShort())
        {
            switch (Settings::settingsType)
            {
            case SettingsType::value:
            {
                incrementTimeOfDayComponent(Settings::timeOfDay, Settings::settingsSelection);
                break;
            }
            case SettingsType::brightness:
            {
                // allow uint8_t overflow
                colorsSettings.at(displayComponentFrom(Settings::settingsSelection)).brightness += 1;
                break;
            }
            case SettingsType::color:
            {
                SelectableColor & colorToModify = colorsSettings.at(displayComponentFrom(Settings::settingsSelection)).selectableColor;
                SelectableColor newColor = nextSelectableColor(colorToModify);
                while (colorsSettings.colorConflicts(newColor))
                {
                    newColor = nextSelectableColor(newColor);
                }
                colorToModify = newColor;
                break;
            }
            }
        }
        else if (Settings::ButtonDown::releasedAfterShort())
        {
            switch (Settings::settingsType)
            {
            case SettingsType::value:
            {
                decrementTimeOfDayComponent(Settings::timeOfDay, Settings::settingsSelection);
                break;
            }
            case SettingsType::brightness:
            {
                // allow uint8_t overflow
                colorsSettings.at(displayComponentFrom(Settings::settingsSelection)).brightness -= 1;
                break;
            }
            case SettingsType::color:
            {
                SelectableColor & colorToModify = colorsSettings.at(displayComponentFrom(Settings::settingsSelection)).selectableColor;
                SelectableColor newColor = previousSelectableColor(colorToModify);
                while (colorsSettings.colorConflicts(newColor))
                {
                    newColor = previousSelectableColor(newColor);
                }
                colorToModify = newColor;
                break;
            }
            }
        }

        if (displayTime)
        {
            // Create color representation.
            showTimeOfDay(strip, Settings::timeOfDay, 0., colorsSettings);

#if PRINT_SERIAL_TIME
            serialPrintTimeOfDay(Settings::timeOfDay);
#endif
        }
    }
    }

    delay(cycleDurationMs);
}
