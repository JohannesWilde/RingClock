#ifndef NEOPIXELPATTERNS_HPP
#define NEOPIXELPATTERNS_HPP

#include <math.h>
#include <Adafruit_NeoPixel.h>

#include "Colors.hpp"


namespace NeoPixelPatterns
{

/**
 * BrightnessFunctionType This type of function shall be used to calculate pixel brightness.
 * Please note, that this does not calculate the brightness of a single pixel value itself,
 * but is supposed to be the analytic integral of the desired color profile.
 * This is so, that for f(x) the pixel brightness of pixel i can be calculated using
 *  brightness(i) = F(i+.5) - F(i-.5),
 * where F(x) is the brightness function [F(x) = int f(x) dx]. Please note that this
 * function should be normalized, so that:
 *  max(brightness(i)) <= 1,
 *  min(brightness(i)) >= 0.
 */
typedef double(*BrightnessFunctionType)(double);

/**
 * @brief brightnessFunctionMountain Integral for f(x) = b/(1 + x^2/a).
 * @param x position
 * @return brightness in [0,1]
 * From normalization 1 != F(.5) - F(-.5) it results:
 *  b = 1/(2*sqrt(a) * atan(1/(2*sqrt(a)))).
 */
double brightnessFunctionMountain(double const x);

/**
 * @brief Integral of delta(x).
 */
double brightnessFunctionDelta(const double x);

/**
 * Normalize a double position with regard to a range.
 * first parameter: position
 * second parameter: range
 */
typedef double(*PositionNormalizationFunctionType)(double, double);

// move position to [0, range)
template<typename T>
T normalizePosition(T const & position, T const & range)
{
    return (((position % range) + range) % range);
}

template<>
double normalizePosition(const double & position, const double & range);

template<>
unsigned normalizePosition(const unsigned & position, const unsigned & range);

template<>
unsigned long normalizePosition(const unsigned long & position, const unsigned long & range);


// symmetrize position to [-range/2, range/2)
// Please note that this requires a static-cast to signed for unsigned types of T.
// This function leaves the 0-position unchanged but wraps around the high half-range to a negative half-range.
// [0, x) -> [-x/2, x/2)
// With x = 4:
// 0 -> 0
// 1 -> 1
// 2 -> -2
// 3 -> -1
template<typename T>
T symmetrizePosition(T const & position, T const & range)
{
    T const normalizedPosition = normalizePosition(position, range);
    T const rangeHalf = range / 2.;
    if (rangeHalf > normalizedPosition)
    {
        return  normalizedPosition;
    }
    else
    {
        return normalizedPosition - range;
    }
}

/* circle back and forth from [0, range) -> [0, range/2]
 *     ^                *
 *   T |    /\          *
 *     |   /  \         *
 *     |  /    \        *
 *     | /      \       *
 *     |/        \      *
 *   0 ------------>    *
 *     0        2T      *
 */
template<typename T>
T circlePosition(T const & position, T const & range)
{
    T const inputRange = 2 * range;
    T const normalizedPosition = normalizePosition(position, inputRange);
    return (normalizedPosition <= range) ? normalizedPosition : (inputRange - normalizedPosition);
}


// position in pixels, i.e. [0, strip.numPixels()).
void addColorsWrapping(Adafruit_NeoPixel & strip,
					   double const position,
					   BrightnessFunctionType brightnessFunction,
					   Colors::Color_t const &color);


} // NeoPixelPatterns

#endif // NEOPIXELPATTERNS_HPP
