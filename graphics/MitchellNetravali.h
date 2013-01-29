/*=====================================================================
MitchellNetravali.h
-------------------
File created by ClassTemplate on Fri Jan 25 04:45:14 2008
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "platform.h"
#include <assert.h>


/*=====================================================================
MitchellNetravali
-----------------

=====================================================================*/
template <class Real>
class MitchellNetravali
{
public:
	/*=====================================================================
	MitchellNetravali
	-----------------
	
	=====================================================================*/
	MitchellNetravali(Real B, Real C);

	~MitchellNetravali();


	// require x >= 0
	INDIGO_STRONG_INLINE Real eval(Real x) const;
	INDIGO_STRONG_INLINE Real evalXSmallerEqual2(Real x) const;

	inline Real getB() const { return B; }
	inline Real getC() const { return C; }


private:
	Real region_0_a, region_0_b, region_0_d; // 0.0 <= x < 1.0
	Real region_1_a, region_1_b, region_1_c, region_1_d; // 1.0 <= x < 2.0
	Real B, C;
};


template <class Real>
MitchellNetravali<Real>::MitchellNetravali(Real B_, Real C_)
:	B(B_), C(C_)
{
	region_0_a = (12 - 9*B - 6*C) / 6;
	region_0_b = (-18 + 12*B + 6*C) / 6;
	region_0_d = (6 - 2*B) / 6;

	region_1_a = (-B - 6*C) / 6;
	region_1_b = (6*B + 30*C) / 6;
	region_1_c = (-12*B - 48*C) / 6;
	region_1_d = (8*B + 24*C) / 6;
}


template <class Real>
MitchellNetravali<Real>::~MitchellNetravali()
{
}


template <class Real>
Real MitchellNetravali<Real>::eval(Real x) const
{
	assert(x >= 0.0);

	if(x < 1)
	{
		// Region '0'
		return region_0_a * x*x*x + region_0_b * x*x + region_0_d;
	}
	else if(x < 2)
	{
		// Region '1'
		return region_1_a * x*x*x + region_1_b * x*x + region_1_c * x + region_1_d;
	}
	else
		return 0;
}


template <class Real>
Real MitchellNetravali<Real>::evalXSmallerEqual2(Real x) const
{
	assert(x >= 0 && x <= 2);

	if(x < 1)
	{
		// Region '0'
		return region_0_a * x*x*x + region_0_b * x*x + region_0_d;
	}
	else
	{
		// Region '1'
		return region_1_a * x*x*x + region_1_b * x*x + region_1_c * x + region_1_d;
	}
}
