#include "Colors.hpp"

namespace // anonymous namespace
{
	
inline uint8_t scaleColorPart(uint8_t const input, double const & scaleFactor)
{
    // this uses approximately 1200 bytes of flash storage!
	double const scaledValue = static_cast<double>(input) * scaleFactor;
	if (255. < scaledValue)
	{
		return 255;
	}
	else if (0 > scaledValue)
	{
		return 0;
	}
	else
	{
		return static_cast<uint8_t>(scaledValue);
	}
}

inline uint8_t addColorPart(uint8_t const one, uint8_t const two)
{
  if ((255 - one) < two)
	{
		return 255; // saturate if summation would overflow
	}
	else
	{
		return one + two;
	}
}

} // anonymous namespace

namespace Colors
{

Color_t colorScaleBrightness(Color_t const & input, double const & scaleFactor)
{
    return Colors::Color(scaleColorPart(input >> 16, scaleFactor),
                         scaleColorPart(input >> 8, scaleFactor),
                         scaleColorPart(input >> 0, scaleFactor),
                         scaleColorPart(input >> 24, scaleFactor));
}

Color_t addColors(Color_t const & one, Color_t const & two)
{
    return Colors::Color(addColorPart(one >> 16, two >> 16),
                         addColorPart(one >> 8, two >> 8),
                         addColorPart(one >> 0, two >> 0),
                         addColorPart(one >> 24, two >> 24));
}

}
