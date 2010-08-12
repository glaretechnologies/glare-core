/*=====================================================================

=====================================================================*/
#ifndef __FILTERFUNCTION_H_666_
#define __FILTERFUNCTION_H_666_

#include "../utils/refcounted.h"
#include <string>

/*=====================================================================
FilterFunction
--------------
Radially-symmetric filter function interface
=====================================================================*/
class FilterFunction : public RefCounted
{
public:
	FilterFunction();

	virtual ~FilterFunction();

	virtual double supportRadius() const = 0;

	virtual double eval(double r) const = 0;

	int getFilterSpan(int supersample_factor);
	float* getFilterData(int supersample_factor);

	virtual const std::string description() const = 0;

private:

	int cached_supersample_factor;
	float* cached_filter_data;
};



#endif //__FILTERFUNCTION_H_666_




