#include "BoxFilterFunction.h"

#include "../utils/stringutils.h"

BoxFilterFunction::BoxFilterFunction()
{}

BoxFilterFunction::~BoxFilterFunction()
{
}

double BoxFilterFunction::supportRadius() const
{
	return 0.5;
}

double BoxFilterFunction::eval(double abs_x) const
{
	return abs_x < 0.5 ? 1.0 : 0.0;
}

const std::string BoxFilterFunction::description() const
{
	return "box";
}
