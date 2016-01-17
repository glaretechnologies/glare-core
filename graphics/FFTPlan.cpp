/*=====================================================================
FFTPlan.cpp
-----------
File created by ClassTemplate on Tue Nov 17 13:29:13 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "FFTPlan.h"




FFTPlan::FFTPlan()
{
	//padded_in = 0;
	//padded_filter = 0;

	buffer_a = 0;
	buffer_b = 0;
	product = 0;

	/*in_plan = 0;
	filter_plan = 0;
	ift_plan = 0;
*/
	failed_to_allocate_buffers = false;
}


FFTPlan::~FFTPlan()
{
	//if(padded_in) fftss_free(padded_in);
	//if(padded_filter) fftss_free(padded_filter);
	/*if(buffer_a) fftss_free(buffer_a);
	if(buffer_b) fftss_free(buffer_b);
	if(product) fftss_free(product);

	if(in_plan) fftss_destroy_plan(in_plan);
	if(filter_plan) fftss_destroy_plan(filter_plan);
	if(ift_plan) fftss_destroy_plan(ift_plan);*/

}






