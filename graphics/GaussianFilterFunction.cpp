/*=====================================================================
GaussianFilterFunction.cpp
--------------------------
File created by ClassTemplate on Thu Feb 28 03:13:54 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "GaussianFilterFunction.h"

#include "../utils/stringutils.h"
#include "../maths/mathstypes.h"



GaussianFilterFunction::GaussianFilterFunction(double standard_dev_)
:	standard_dev(standard_dev_)
{
	
}


GaussianFilterFunction::~GaussianFilterFunction()
{
	
}



double GaussianFilterFunction::supportRadius() const
{
	const double GAUSSIAN_THRESHOLD = 1.0e-5;
	return Maths::inverse1DGaussian(
		GAUSSIAN_THRESHOLD,
		standard_dev
		);
}

double GaussianFilterFunction::eval(double abs_x) const
{
	return Maths::eval1DGaussian(abs_x, standard_dev);
}

const std::string GaussianFilterFunction::description() const
{
	return "Gaussian, standard deviation=" + toString(standard_dev) + ", support radius=" + toString(supportRadius());
}


