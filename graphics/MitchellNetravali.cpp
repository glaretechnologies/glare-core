/*=====================================================================
MitchellNetravali.cpp
---------------------
File created by ClassTemplate on Fri Jan 25 04:45:14 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "MitchellNetravali.h"




MitchellNetravali::MitchellNetravali(double B_, double C_)
:	B(B_), C(C_)
{
	region_0_a = (12.0 - 9.0*B - 6.0*C) / 6.0;
	region_0_b = (-18.0 + 12.0*B + 6.0*C) / 6.0;
	region_0_d = (6.0 - 2.0*B) / 6.0;

	region_1_a = (-B - 6.0*C) / 6.0;
	region_1_b = (6.0*B + 30.0*C) / 6.0;
	region_1_c = (-12.0*B - 48.0*C) / 6.0;
	region_1_d = (8.0*B + 24.0*C) / 6.0;
}


MitchellNetravali::~MitchellNetravali()
{
	
}






