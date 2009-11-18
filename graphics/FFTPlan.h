/*=====================================================================
FFTPlan.h
---------
File created by ClassTemplate on Tue Nov 17 13:29:13 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __FFTPLAN_H_666_
#define __FFTPLAN_H_666_


#include "fftss.h"


/*=====================================================================
FFTPlan
-------

=====================================================================*/
class FFTPlan
{
public:
	/*=====================================================================
	FFTPlan
	-------
	
	=====================================================================*/
	FFTPlan();

	~FFTPlan();


	// Aligned arrays
	//double* padded_in;
	//double* padded_filter;
	//double* in_ft;
	//double* filter_ft;
	//double* product;
	double* buffer_a;
	double* buffer_b;
	double* product;


	fftss_plan in_plan;
	fftss_plan filter_plan;
	fftss_plan ift_plan;

	bool failed_to_allocate_buffers;
};



#endif //__FFTPLAN_H_666_




