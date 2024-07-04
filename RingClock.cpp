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

#include "eeprom.hpp"

#include "helpers/statemachine.hpp"
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


struct BackupValues
{
    ColorsSettings colorsSettings;

    BackupValues(ColorsSettings const & colorsSettings)
        : colorsSettings(colorsSettings)
    {
        // intentionally empty
    }
};

static Eeprom::Address constexpr backupValuesAddress = 0;
static_assert(Eeprom::fitsInEeprom<BackupValues, backupValuesAddress>());


// Static variables and instances.

namespace Pins
{
int constexpr led = 3;
}

uint16_t constexpr ledCount = 12;
uint8_t constexpr defaultMaxBrightness = 200;

#define PRINT_SERIAL_TIME false
#define PRINT_SERIAL_BUTTONS false

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

template<uint8_t Index>
struct WrapperCountIsDown
{
    static void impl(uint8_t & count)
    {
        if (Buttons<Index>::isDown())
        {
            ++count;
        }
    }
};

uint8_t getButtonsAreDown()
{
    uint8_t count = 0;
    Helpers::TMP::Loop<4, WrapperCountIsDown, uint8_t &>::impl(count);
    return count;
};


// Statemachine classes and "static const virtual" instances.

// forward declaration
struct DataClock;

class SettingsClockDisplay
{
private:
    friend class StateClockDisplay;

    static uint8_t constexpr previousSecondsInvalid = 255;

    uint8_t previousSeconds = previousSecondsInvalid;
    unsigned long lastSecondChangeTime = 0;

    bool modeChangeButtonWasUpOnceInThisMode = true;
};

class SettingsModify
{
private:
    friend class StateClockSettings;
    friend class StateModifyValue;
    friend class StateModifyBrightness;
    friend class StateModifyColor;

    SettingsSelection settingsSelection = SettingsSelection::hours;
    uint8_t longPressDurationAccumulation = 0;
};

class SettingsClockSettings
{
private:
    friend class StateClockSettings;

    Helpers::Statemachine<DataClock> statemachineModify{Helpers::Statemachine<DataClock>::noopState};

    bool modeChangeButtonWasUpOnceInThisMode = true;

    SettingsModify settingsModify;
};

struct DataClock
{
    TimeOfDay timeOfDay;
    double subseconds = 0;
    ColorsSettings colorsSettings;
    bool updateDisplay = false;

    SettingsClockDisplay settingsClockDisplay;
    SettingsClockSettings settingsClockSettings;
}; // namespace Common


class StateClockDisplay : public Helpers::AbstractState<DataClock>
{
public:
    // typedef ButtonTop;
    typedef ButtonRight ButtonSettings;
    // typedef ButtonBottom;
    // typedef ButtonLeft;

    void init(DataClock & data) const override
    {
        data.settingsClockDisplay.previousSeconds = data.settingsClockDisplay.previousSecondsInvalid;
        data.settingsClockDisplay.lastSecondChangeTime = 0;

        data.settingsClockDisplay.modeChangeButtonWasUpOnceInThisMode = false;
    }

    AbstractState const & process(DataClock & data) const override;

    void deinit(DataClock & data) const override
    {
        // intentionally empty
    }
};
static StateClockDisplay stateClockDisplay;


class StateClockSettings : public Helpers::AbstractState<DataClock>
{
public:
    typedef ButtonTop ButtonUp;
    typedef ButtonRight ButtonSelectOrExit;
    typedef ButtonBottom ButtonDown;
    typedef ButtonLeft ButtonBrightnessOrColor;

    void init(DataClock & data) const override;

    AbstractState const & process(DataClock & data) const override;

    void deinit(DataClock & data) const override
    {
        // intentionally empty
    }

protected:
    static SettingsModify & getSettingsModify(DataClock & data)
    {
        return data.settingsClockSettings.settingsModify;
    }
};
static StateClockSettings stateClockSettings;


class StateModifyValue : public StateClockSettings
{
public:

    void init(DataClock & data) const override
    {
        getSettingsModify(data).longPressDurationAccumulation = 0;
    }

    AbstractState const & process(DataClock & data) const override;

    void deinit(DataClock & data) const override
    {
        // intentionally empty
    }
};
static StateModifyValue stateModifyValue;


class StateModifyBrightness : public StateClockSettings
{
public:

    void init(DataClock & data) const override
    {
        getSettingsModify(data).longPressDurationAccumulation = 0;
    }

    AbstractState const & process(DataClock & data) const override;

    void deinit(DataClock & data) const override
    {
        // intentionally empty
    }
};
static StateModifyBrightness stateModifyBrightness;


class StateModifyColor : public StateClockSettings
{
public:

    void init(DataClock & data) const override
    {
        getSettingsModify(data).longPressDurationAccumulation = 0;
    }

    AbstractState const & process(DataClock & data) const override;

    void deinit(DataClock & data) const override
    {
        // intentionally empty
    }
};
static StateModifyColor stateModifyColor;


// Statemachine instance data.

static DataClock dataClock;
static Helpers::Statemachine<DataClock> statemachine(stateClockDisplay);


// setup() and loop() functionality.

void setup()
{
    // Power for the RTC, as I don't have enough 5V ports on the UNO.
    PinPowerRtc::initialize<AvrInputOutput::PinState::High>();

    Helpers::TMP::Loop<4, WrapperInitialize>::impl();

    // Start the I2C interface
    // For an Arduino Uno this entails: A4 - SDA, A5 - SCL
    Wire.begin();

    // Use 12h-Mode.
    myRTC.setClockMode(true);

    // Startup LEDs
    strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
    strip.show();            // Turn OFF all pixels ASAP
    strip.setBrightness(255);

    // Todo: save and load settings from eeprom
    BackupValues backupValues(dataClock.colorsSettings);
    bool const readBack = Eeprom::readWithCrc(&backupValues, sizeof(BackupValues), backupValuesAddress);
    if (readBack)
    {
        dataClock.colorsSettings = backupValues.colorsSettings;
    }
    else
    {
        dataClock.colorsSettings.at(DisplayComponent::hours).brightness = defaultMaxBrightness;
        dataClock.colorsSettings.at(DisplayComponent::hours).selectableColor = SelectableColor::blue;
        dataClock.colorsSettings.at(DisplayComponent::minutes).brightness = defaultMaxBrightness;
        dataClock.colorsSettings.at(DisplayComponent::minutes).selectableColor = SelectableColor::green;
        dataClock.colorsSettings.at(DisplayComponent::seconds).brightness = defaultMaxBrightness;
        dataClock.colorsSettings.at(DisplayComponent::seconds).selectableColor = SelectableColor::red;
    }

#if PRINT_SERIAL_TIME || PRINT_SERIAL_BUTTONS
    // Start the serial interface
    Serial.begin(57600);
#endif
}


void loop()
{
    Helpers::TMP::Loop<4, WrapperUpdate>::impl();

#if PRINT_SERIAL_BUTTONS
    serialPrintButton<ButtonTop>("ButtonTop");
    serialPrintButton<ButtonRight>("ButtonRight");
    serialPrintButton<ButtonBottom>("ButtonBottom");
    serialPrintButton<ButtonLeft>("ButtonLeft");
#endif

    // Assume update to always be necessary - state must opt-out explicitely.
    dataClock.updateDisplay = true;

    statemachine.process(dataClock);

    if (dataClock.updateDisplay)
    {
        // Create color representation.
        showTimeOfDay(strip, dataClock.timeOfDay, dataClock.subseconds, dataClock.colorsSettings);
    }

#if PRINT_SERIAL_TIME
    serialPrintTimeOfDay(timeOfDay);
#endif

    delay(cycleDurationMs);
}



Helpers::AbstractState<DataClock> const & StateClockDisplay::process(DataClock & data) const
{
    Helpers::AbstractState<DataClock> const * nextState = this;

    if (!data.settingsClockDisplay.modeChangeButtonWasUpOnceInThisMode)
    {
        if (StateClockSettings::ButtonSelectOrExit::isUp())
        {
            data.settingsClockDisplay.modeChangeButtonWasUpOnceInThisMode = true;
        }
    }
    else if (ButtonSettings::isDownLong())
    {
        data.timeOfDay = getTimeOfDayFromRTC(myRTC);

        data.updateDisplay = false;

        nextState = &stateClockSettings;
    }

    if (data.updateDisplay)
    {
        // Get hour, minutes, and seconds from RTC.
        data.timeOfDay = getTimeOfDayFromRTC(myRTC);

        // simulate subseconds
        double subseconds = 0.;
        if (data.settingsClockDisplay.previousSeconds != data.timeOfDay.seconds)
        {
            // seconds changed -> reset subseconds to 0
            data.settingsClockDisplay.lastSecondChangeTime = millis();
            data.settingsClockDisplay.previousSeconds = data.timeOfDay.seconds;
        }
        else
        {
            // simulate via millis()
            subseconds = static_cast<double>(millis() - data.settingsClockDisplay.lastSecondChangeTime) / 1000.;
        }
    }

    return *nextState;
}

void StateClockSettings::init(DataClock & data) const
{
    data.settingsClockSettings.settingsModify.settingsSelection = SettingsSelection::hours;
    data.settingsClockSettings.statemachineModify.reset(data, stateModifyValue);

    data.settingsClockSettings.modeChangeButtonWasUpOnceInThisMode = false;

    data.subseconds = 0;
}

Helpers::AbstractState<DataClock> const & StateClockSettings::process(DataClock & data) const
{
    Helpers::AbstractState<DataClock> const * nextState = this;

    if (!data.settingsClockSettings.modeChangeButtonWasUpOnceInThisMode)
    {
        if (StateClockDisplay::ButtonSettings::isUp())
        {
            data.settingsClockSettings.modeChangeButtonWasUpOnceInThisMode = true;
        }
    }
    else if (StateClockSettings::ButtonSelectOrExit::isDownLong() && (1 == getButtonsAreDown()))
    {
        BackupValues const backupValues(data.colorsSettings);
        Eeprom::writeWithCrc(&backupValues, sizeof(BackupValues), backupValuesAddress);

        nextState = &stateClockDisplay;

        myRTC.setSecond(data.timeOfDay.seconds);
        myRTC.setMinute(data.timeOfDay.minutes);
        myRTC.setHour(data.timeOfDay.hours);
        // myRTC.setDoW(7);
        // myRTC.setDate(9);
        // myRTC.setMonth(6);
        // myRTC.setYear(24);
#if PRINT_SERIAL_TIME
        Serial.print("Settings: ");
        serialPrintTimeOfDay(data.timeOfDay);
#endif

        data.updateDisplay = false;
    }
    else
    {
        data.settingsClockSettings.statemachineModify.process(data);
    }

    return *nextState;
}

Helpers::AbstractState<DataClock> const & StateModifyValue::process(DataClock & data) const
{
    Helpers::AbstractState<DataClock> const * nextState = this;

    uint8_t const numberOfButtonsAreDown = getButtonsAreDown();

    if (1 < numberOfButtonsAreDown)
    {
        // In this mode at most  1 button is supposed to be pressed at the same time.
        // Reset accumulation counter but do nothing else if more than 1 buttons are pressed.
        getSettingsModify(data).longPressDurationAccumulation = 0;
    }
    else
    {
        if (0 == numberOfButtonsAreDown)
        {
            getSettingsModify(data).longPressDurationAccumulation = 0;
        }
        else if (StateClockSettings::ButtonBrightnessOrColor::isDownLong())
        {
            nextState = &stateModifyBrightness;
        }
        else if (StateClockSettings::ButtonSelectOrExit::pressed())
        {
            getSettingsModify(data).settingsSelection = nextSettingsSelection(getSettingsModify(data).settingsSelection);
        }
        else if (StateClockSettings::ButtonUp::isDownLong())
        {
            getSettingsModify(data).longPressDurationAccumulation = incrementUint8Capped(getSettingsModify(data).longPressDurationAccumulation);
            if (longPressDurationActive(getSettingsModify(data).longPressDurationAccumulation))
            {
                incrementTimeOfDayComponent(data.timeOfDay, getSettingsModify(data).settingsSelection);
            }
        }
        else if (StateClockSettings::ButtonDown::isDownLong())
        {
            getSettingsModify(data).longPressDurationAccumulation = incrementUint8Capped(getSettingsModify(data).longPressDurationAccumulation);
            if (longPressDurationActive(getSettingsModify(data).longPressDurationAccumulation))
            {
                decrementTimeOfDayComponent(data.timeOfDay, getSettingsModify(data).settingsSelection);
            }
        }

        if (StateClockSettings::ButtonBrightnessOrColor::releasedAfterShort())
        {
            nextState = &stateModifyColor;
        }

        if (StateClockSettings::ButtonUp::releasedAfterShort())
        {
            incrementTimeOfDayComponent(data.timeOfDay, getSettingsModify(data).settingsSelection);
        }

        if (StateClockSettings::ButtonDown::releasedAfterShort())
        {
            decrementTimeOfDayComponent(data.timeOfDay, getSettingsModify(data).settingsSelection);
        }
    }

    return *nextState;
}

Helpers::AbstractState<DataClock> const & StateModifyBrightness::process(DataClock & data) const
{
    Helpers::AbstractState<DataClock> const * nextState = this;

    uint8_t const numberOfButtonsAreDown = getButtonsAreDown();

    if (!StateClockSettings::ButtonBrightnessOrColor::isDownLong())
    {
        // As long as brightness is to be modified, ButtonBrightnessOrColor needs to stay pressed down.
        // Reset accumulation counter but do nothing else if another button is still pressed.
        getSettingsModify(data).longPressDurationAccumulation = 0;

        // In order to return no other button must be pressed.
        if (0 == numberOfButtonsAreDown)
        {
            nextState = &stateModifyValue;
        }
    }
    else
    {
        if (1 == numberOfButtonsAreDown)
        {
            // All but ButtonBrightnessOrColor released.
            getSettingsModify(data).longPressDurationAccumulation = 0;
        }
        else if (StateClockSettings::ButtonSelectOrExit::pressed())
        {
            getSettingsModify(data).settingsSelection = nextSettingsSelection(getSettingsModify(data).settingsSelection);
        }
        else if (StateClockSettings::ButtonUp::isDownLong())
        {
            getSettingsModify(data).longPressDurationAccumulation = incrementUint8Capped(getSettingsModify(data).longPressDurationAccumulation);
            if (longPressDurationActive(getSettingsModify(data).longPressDurationAccumulation))
            {
                // allow uint8_t overflow
                data.colorsSettings.at(displayComponentFrom(getSettingsModify(data).settingsSelection)).brightness += 2;
            }
        }
        else if (StateClockSettings::ButtonDown::isDownLong())
        {
            getSettingsModify(data).longPressDurationAccumulation = incrementUint8Capped(getSettingsModify(data).longPressDurationAccumulation);
            if (longPressDurationActive(getSettingsModify(data).longPressDurationAccumulation))
            {
                // allow uint8_t underflow
                data.colorsSettings.at(displayComponentFrom(getSettingsModify(data).settingsSelection)).brightness -= 2;
            }
        }

        if (StateClockSettings::ButtonUp::releasedAfterShort())
        {
            // allow uint8_t overflow
            data.colorsSettings.at(displayComponentFrom(getSettingsModify(data).settingsSelection)).brightness += 1;
        }

        if (StateClockSettings::ButtonDown::releasedAfterShort())
        {
            // allow uint8_t underflow
            data.colorsSettings.at(displayComponentFrom(getSettingsModify(data).settingsSelection)).brightness -= 1;
        }
    }

    return *nextState;
}

Helpers::AbstractState<DataClock> const & StateModifyColor::process(DataClock & data) const
{
    Helpers::AbstractState<DataClock> const * nextState = this;

    uint8_t const numberOfButtonsAreDown = getButtonsAreDown();

    if (1 < numberOfButtonsAreDown)
    {
        // In this mode at most 1 button is supposed to be pressed at the same time.
        // Reset accumulation counter but do nothing else if more than 1 buttons are pressed.
        getSettingsModify(data).longPressDurationAccumulation = 0;
    }
    else
    {
        if (0 == numberOfButtonsAreDown)
        {
            getSettingsModify(data).longPressDurationAccumulation = 0;
        }
        else if (StateClockSettings::ButtonBrightnessOrColor::isDownLong())
        {
            nextState = &stateModifyBrightness;
        }
        else if (StateClockSettings::ButtonSelectOrExit::pressed())
        {
            getSettingsModify(data).settingsSelection = nextSettingsSelection(getSettingsModify(data).settingsSelection);
        }
        else if (StateClockSettings::ButtonUp::isDownLong())
        {
            getSettingsModify(data).longPressDurationAccumulation = incrementUint8Capped(getSettingsModify(data).longPressDurationAccumulation);
            if (longPressDurationActive(getSettingsModify(data).longPressDurationAccumulation))
            {
                SelectableColor & colorToModify = data.colorsSettings.at(displayComponentFrom(getSettingsModify(data).settingsSelection)).selectableColor;
                SelectableColor newColor = nextSelectableColor(colorToModify);
                while (data.colorsSettings.colorConflicts(newColor))
                {
                    newColor = nextSelectableColor(newColor);
                }
                colorToModify = newColor;
            }
        }
        else if (StateClockSettings::ButtonDown::isDownLong())
        {
            getSettingsModify(data).longPressDurationAccumulation = incrementUint8Capped(getSettingsModify(data).longPressDurationAccumulation);
            if (longPressDurationActive(getSettingsModify(data).longPressDurationAccumulation))
            {
                SelectableColor & colorToModify = data.colorsSettings.at(displayComponentFrom(getSettingsModify(data).settingsSelection)).selectableColor;
                SelectableColor newColor = previousSelectableColor(colorToModify);
                while (data.colorsSettings.colorConflicts(newColor))
                {
                    newColor = previousSelectableColor(newColor);
                }
                colorToModify = newColor;
            }
        }


        if (StateClockSettings::ButtonBrightnessOrColor::releasedAfterShort())
        {
            nextState = &stateModifyValue;
        }

        if (StateClockSettings::ButtonUp::releasedAfterShort())
        {
            SelectableColor & colorToModify = data.colorsSettings.at(displayComponentFrom(getSettingsModify(data).settingsSelection)).selectableColor;
            SelectableColor newColor = nextSelectableColor(colorToModify);
            while (data.colorsSettings.colorConflicts(newColor))
            {
                newColor = nextSelectableColor(newColor);
            }
            colorToModify = newColor;
        }

        if (StateClockSettings::ButtonDown::releasedAfterShort())
        {
            SelectableColor & colorToModify = data.colorsSettings.at(displayComponentFrom(getSettingsModify(data).settingsSelection)).selectableColor;
            SelectableColor newColor = previousSelectableColor(colorToModify);
            while (data.colorsSettings.colorConflicts(newColor))
            {
                newColor = previousSelectableColor(newColor);
            }
            colorToModify = newColor;
        }
    }

    return *nextState;
}
