#include "SharpFilterFunction.h"

#include "../utils/stringutils.h"


SharpFilterFunction::SharpFilterFunction()
:	mn(0.2, 0.4)
{

}


SharpFilterFunction::~SharpFilterFunction()
{

}


double SharpFilterFunction::supportRadius() const
{
	return 2.5;
}


double SharpFilterFunction::eval(double r_01) const
{
	const double r_MN = r_01 * 2.0; // Mitchell-Netravali filter defined for r in [0, 2]

	return mn.eval(r_MN);
}


const std::string SharpFilterFunction::description() const
{
	return "sharp";
}
