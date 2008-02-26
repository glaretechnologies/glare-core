#include "MitchellNetravaliFilterFunction.h"

#include "../utils/stringutils.h"

MitchellNetravaliFilterFunction::MitchellNetravaliFilterFunction(double B, double C)
:	mn(B, C)
{}

MitchellNetravaliFilterFunction::~MitchellNetravaliFilterFunction()
{
}

double MitchellNetravaliFilterFunction::supportRadius() const
{
	return 2.0;
}

double MitchellNetravaliFilterFunction::eval(double abs_x) const
{
	return mn.eval(abs_x);
}

const std::string MitchellNetravaliFilterFunction::description() const
{
	return "mn_cubic, blur=" + toString(mn.getB()) + ", ring=" + toString(mn.getC());
}