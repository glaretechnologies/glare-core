/*=====================================================================

=====================================================================*/
#pragma once

#include "FilterFunction.h"
#include "MitchellNetravali.h"

/*=====================================================================
SharpFilterFunction
-------------------
=====================================================================*/
class SharpFilterFunction : public FilterFunction
{
public:
	SharpFilterFunction();
	virtual ~SharpFilterFunction();

	virtual double supportRadius() const;

	virtual double eval(double abs_x) const;

	virtual const std::string description() const;

private:
	MitchellNetravali<double> mn;
};
