#include "NeoPixelPatterns.hpp"

namespace NeoPixelPatterns
{

double brightnessFunctionMountain(const double x)
{
    double constexpr halfWidthHalfMaximum = .01;
    double constexpr halfWidthHalfMaximumSqrt = sqrt(halfWidthHalfMaximum);
    double constexpr normalization = 2. * atan(1./(2 * halfWidthHalfMaximumSqrt));
    return atan(x/halfWidthHalfMaximumSqrt) / normalization;
}

double brightnessFunctionDelta(const double x)
{
    return (x > 0.) ? 1. : 0.;
}

template<>
double normalizePosition(const double & position, const double & range)
{
    return fmod(fmod(position, range) + range, range);
}

template<>
unsigned normalizePosition(const unsigned & position, const unsigned & range)
{
    return (position % range);
}

template<>
unsigned long normalizePosition(const unsigned long & position, const unsigned long & range)
{
    return (position % range);
}


void addColorsWrapping(Adafruit_NeoPixel & strip,
					   double const position,
					   BrightnessFunctionType brightnessFunction,
					   Colors::Color_t const &color)
{
    double const numberOfPixelsDouble = static_cast<double>(strip.numPixels());
	  double previousPosition = symmetrizePosition(-1. * position - .5, numberOfPixelsDouble);
    double previousBrightness = brightnessFunction(previousPosition);
    for (unsigned i = 0; i < strip.numPixels(); ++i)
    {
        double const nextPosition = symmetrizePosition(previousPosition + 1., numberOfPixelsDouble);
        double const nextBrightness = brightnessFunction(nextPosition);
        // Where the brightness wraps around, previousBrightness has to be recalculated.
        if (nextPosition < previousPosition)
        {
            previousBrightness = brightnessFunction(nextPosition - 1.);
        }

        // As written above: brightness = F(i+.5) - F(i-.5)
        double const brightness = nextBrightness - previousBrightness;

        Colors::Color_t const newColor = Colors::colorScaleBrightness(color, brightness);
		
		    strip.setPixelColor(i, Colors::addColors(newColor, strip.getPixelColor(i)));

        previousBrightness = nextBrightness;
		    previousPosition = nextPosition;
    }
}

} // NeoPixelPatterns
