#include "MitchellNetravaliFilterFunction.h"

#include "../utils/stringutils.h"


MitchellNetravaliFilterFunction::MitchellNetravaliFilterFunction(double B_, double C_)
:	mn(B_, C_)
{
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
	return 2.0;
}


double MitchellNetravaliFilterFunction::eval(double r_01) const
{
	const double r_MN = r_01 * 2.0; // Mitchell-Netravali filter defined for r in [0, 2]

	return mn.eval(r_MN);
}


const std::string MitchellNetravaliFilterFunction::description() const
{
	return "mn_cubic, blur=" + toString(mn.getB()) + ", ring=" + toString(mn.getC()) + ", support=" + toString(supportRadius()) + "px";
}
