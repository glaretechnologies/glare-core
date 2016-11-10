/*=====================================================================
FilterFunction.h
----------------
Copyright Glare Technologies Limited 2016 -
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

	virtual const std::string description() const = 0;


	int getFilterSpan(int supersample_factor) const;
	
	void getFilterDataVec(int supersample_factor, std::vector<float>& filter_data_out) const;
};
