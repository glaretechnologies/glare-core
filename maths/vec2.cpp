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

const std::string Vec2<float>::toString() const
{
	const int num_dec_places = 4;
	return "(" + floatToString(x, num_dec_places) + "," + floatToString(y, num_dec_places) + ")";
}
const std::string Vec2<double>::toString() const
{
	const int num_dec_places = 4;
	return "(" + floatToString(x, num_dec_places) + "," + floatToString(y, num_dec_places) + ")";
}
