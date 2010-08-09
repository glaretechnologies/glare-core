/*=====================================================================

=====================================================================*/
#ifndef __MitchellNetravaliFilterFunction_H_666_
#define __MitchellNetravaliFilterFunction_H_666_

#include "FilterFunction.h"
#include "MitchellNetravali.h"

/*=====================================================================
FilterFunction
--------
Separable filter function interface
=====================================================================*/
class MitchellNetravaliFilterFunction : public FilterFunction
{
public:
	MitchellNetravaliFilterFunction(double B_, double C_);

	virtual ~MitchellNetravaliFilterFunction();

	void getParameters(double* B_out, double* C_out);

	virtual double supportRadius() const;

	virtual double eval(double abs_x) const;

	virtual const std::string description() const;

private:
	MitchellNetravali<double> mn;
};



#endif //__MitchellNetravaliFilterFunction_H_666_




