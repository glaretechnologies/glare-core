/*=====================================================================
NVec.cpp
--------
File created by ClassTemplate on Thu Aug 24 19:52:33 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "NVec.h"

#include <assert.h>

/*
NVec::NVec()
{
	
}


NVec::~NVec()
{
	
}
*/


void unitTestNVec()
{

	const int N = 5;
	NVec<N, double> v(1.);
	assert(v.size() == N);

	assert(v[0] == 1.);
	assert(v[4] == 1.);

	v.set(2.);
	assert(v[0] == 2.);
	assert(v[4] == 2.);

	assert(v == (NVec<N, double>(2.)));

	v.set(1.);
	
	v *= 100.f;
	assert(epsEqual(v[4], 100.));

	v.setToMult(NVec<N, double>(2.), 3.);
	assert(epsEqual(v[0], 6.));
	assert(epsEqual(v[4], 6.));

	v.addMult(NVec<N, double>(2.), 1.);
	assert(epsEqual(v[0], 8.));
	assert(epsEqual(v[4], 8.));

	v += v;
	assert(epsEqual(v[0], 16.));
	assert(epsEqual(v[4], 16.));

	v *= (1. / v.maxVal());
	assert(epsEqual(v[0], 1.));
	v.set(1.0);
	assert(v.minVal() == v.maxVal() == 1.0);
	v[0] = 0.;
	v[1] = 1.;
	v[2] = 2.;
	v[3] = 3.;
	v[4] = 4.;
	assert(v.minVal() == 0.0);
	assert(v.maxVal() == 4.0);
	assert(epsEqual(v.sum(), 10.0));

	v = NVec<N, double>(2.);
	assert(v.maxVal() == 2.);
}
















