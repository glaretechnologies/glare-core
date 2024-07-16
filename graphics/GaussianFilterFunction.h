/*=====================================================================
GaussianFilterFunction.h
------------------------
File created by ClassTemplate on Thu Feb 28 03:13:54 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __GAUSSIANFILTERFUNCTION_H_666_
#define __GAUSSIANFILTERFUNCTION_H_666_


#include "FilterFunction.h"



/*=====================================================================
GaussianFilterFunction
----------------------

=====================================================================*/
class GaussianFilterFunction final : public FilterFunction
{
public:
	/*=====================================================================
	GaussianFilterFunction
	----------------------
	
	=====================================================================*/
	GaussianFilterFunction(double standard_dev);

	virtual ~GaussianFilterFunction();

	virtual double supportRadius() const;

	virtual double eval(double abs_x) const;

	virtual const std::string description() const;

private:
	double standard_dev;
};



#endif //__GAUSSIANFILTERFUNCTION_H_666_




