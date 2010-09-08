/*=====================================================================

=====================================================================*/
#ifndef __MitchellNetravaliFilterFunction_H_666_
#define __MitchellNetravaliFilterFunction_H_666_

#include "FilterFunction.h"
#include "MitchellNetravali.h"

/*=====================================================================
MitchellNetravaliFilterFunction
-------------------------------
=====================================================================*/
class MitchellNetravaliFilterFunction : public FilterFunction
{
public:
	MitchellNetravaliFilterFunction(double B_, double C_, double radius = 2.0);

	virtual ~MitchellNetravaliFilterFunction();

	void getParameters(double* B_out, double* C_out);

	virtual double supportRadius() const;

	virtual double eval(double abs_x) const;

	virtual const std::string description() const;

private:
	MitchellNetravali<double> mn;

	double support_radius;
};



#endif //__MitchellNetravaliFilterFunction_H_666_




