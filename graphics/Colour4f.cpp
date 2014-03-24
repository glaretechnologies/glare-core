#include "Colour4f.h"


#include "../utils/StringUtils.h"


const std::string Colour4f::toString() const
{
	return "(" + ::toString(x[0]) + ", " + ::toString(x[1]) + ", " + ::toString(x[2]) + ", " + ::toString(x[3]) + ")";
}
