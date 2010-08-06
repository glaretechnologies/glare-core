#include "MitchellNetravaliFilterFunction.h"

#include "../utils/stringutils.h"


MitchellNetravaliFilterFunction::MitchellNetravaliFilterFunction(double B_, double C_)
:	mn(B, C),
	B(B_),
	C(C_)
{}


MitchellNetravaliFilterFunction::~MitchellNetravaliFilterFunction()
{
}


void MitchellNetravaliFilterFunction::getParameters(double* B_out, double* C_out)
{
	*B_out = B;
	*C_out = C;
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
