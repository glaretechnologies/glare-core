/*=====================================================================

=====================================================================*/
#ifndef __FILTERFUNCTION_H_666_
#define __FILTERFUNCTION_H_666_

#include "../utils/refcounted.h"
#include <string>

/*=====================================================================
FilterFunction
--------
Separable filter function interface
=====================================================================*/
class FilterFunction : public RefCounted
{
public:
	FilterFunction(){}

	virtual ~FilterFunction(){}

	virtual double supportRadius() const = 0;

	virtual double eval(double abs_x) const = 0;

	virtual const std::string description() const = 0;

private:

};



#endif //__FILTERFUNCTION_H_666_




