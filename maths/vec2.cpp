#include "vec2.h"

#include "../utils/random.h"
#include "../utils/stringutils.h"

/*
const Vec2 Vec2::randomVec(float component_lowbound, float component_highbound)
{
	return Vec2(Random::inRange(component_lowbound, component_highbound),
				Random::inRange(component_lowbound, component_highbound));
}

*/

template <>
const std::string Vec2<float>::toString() const
{
	return "(" + floatToString(x) + "," + floatToString(y) + ")";
}

template <>
const std::string Vec2<double>::toString() const
{
	return "(" + doubleToString(x) + "," + doubleToString(y) + ")";
}
