/*=====================================================================
UnitInterpolatedTable.h
-----------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "Array2D.h"
#include "../maths/vec2.h"


/*=====================================================================
UnitInterpolatedTable
---------------------
=====================================================================*/
class UnitInterpolatedTable
{
public:
	typedef float Real;

	UnitInterpolatedTable(const Array2D<Real>& data);
	~UnitInterpolatedTable();

	Real getValue(Real x, Real y) const;
	Real getValueSSE4(Real x, Real y) const;
	const Vec2f getValues(const Vec2f& x_vals, Real y) const;


	const Array2D<Real>& getData() const { return data; }

	Real getInvXStep() const { return recip_x_step; }
	Real getInvYStep() const { return recip_y_step; }

	static void test();

private:
	Array2D<Real> data;
	Real recip_x_step;
	Real recip_y_step;
	int width_minus_1;
	int height_minus_1;
	int width;
	int height;
};
