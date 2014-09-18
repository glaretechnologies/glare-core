#include "MitchellNetravaliFilterFunction.h"

#include "../utils/StringUtils.h"


MitchellNetravaliFilterFunction::MitchellNetravaliFilterFunction(double B_, double C_, double radius/* = 2.0*/)
:	mn(B_, C_),
	support_radius(radius)
{
	assert((radius > 0) && (radius < 8));
}


MitchellNetravaliFilterFunction::~MitchellNetravaliFilterFunction()
{
}


void MitchellNetravaliFilterFunction::getParameters(double* B_out, double* C_out)
{
	*B_out = mn.getB();
	*C_out = mn.getC();
}


double MitchellNetravaliFilterFunction::supportRadius() const
{
	return support_radius;
}


double MitchellNetravaliFilterFunction::eval(double r_01) const
{
	const double r_MN = r_01 * 2.0; // Mitchell-Netravali filter defined for r in [0, 2]

	return mn.eval(r_MN);
}


const std::string MitchellNetravaliFilterFunction::description() const
{
	return "mn_cubic, blur=" + doubleToStringNDecimalPlaces(mn.getB(), 3) + ", ring=" + doubleToStringNDecimalPlaces(mn.getC(), 3) + ", support=" + doubleToStringNDecimalPlaces(supportRadius() * 2, 3) + "px";
}
