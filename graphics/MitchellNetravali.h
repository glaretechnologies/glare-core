/*=====================================================================
MitchellNetravali.h
-------------------
File created by ClassTemplate on Fri Jan 25 04:45:14 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MITCHELLNETRAVALI_H_666_
#define __MITCHELLNETRAVALI_H_666_


#include <assert.h>


/*=====================================================================
MitchellNetravali
-----------------

=====================================================================*/
class MitchellNetravali
{
public:
	/*=====================================================================
	MitchellNetravali
	-----------------
	
	=====================================================================*/
	MitchellNetravali(double B, double C);

	~MitchellNetravali();


	// require x >= 0
	inline double eval(double x) const;

	inline double getB() const { return B; }
	inline double getC() const { return C; }


private:
	double region_0_a, region_0_b, region_0_d; // 0.0 <= x < 1.0
	double region_1_a, region_1_b, region_1_c, region_1_d; // 1.0 <= x < 2.0
	double B, C;
};


double MitchellNetravali::eval(double x) const
{
	assert(x >= 0.0);

	if(x < 1.0)
	{
		// Region '0'
		return region_0_a * x*x*x + region_0_b * x*x + region_0_d;
	}
	else if(x < 2.0)
	{
		// Region '1'
		return region_1_a * x*x*x + region_1_b * x*x + region_1_c * x + region_1_d;
	}
	else
		return 0.0;
}


#endif //__MITCHELLNETRAVALI_H_666_




