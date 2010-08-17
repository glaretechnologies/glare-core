#include "timer.h"


#include "stringutils.h"


const std::string Timer::elapsedString() const // e.g "30.4 s"
{
	return toString(this->elapsed()) + " s";
}
