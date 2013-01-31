/*=====================================================================

=====================================================================*/
#pragma once


#include <vector>
#include <string>
#include "../utils/RefCounted.h"

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

	int getFilterSpan(int supersample_factor) const;
	float* getFilterData(int supersample_factor);
	const std::vector<float> getFilterDataVec(int supersample_factor) const;

	virtual const std::string description() const = 0;

private:

	std::vector<float> filter_data;
};
