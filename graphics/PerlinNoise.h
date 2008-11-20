/*=====================================================================
PerlinNoise.h
-------------
File created by ClassTemplate on Fri Jun 08 01:50:46 2007
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PERLINNOISE_H_666_
#define __PERLINNOISE_H_666_


/*=====================================================================
PerlinNoise
-----------

=====================================================================*/
class PerlinNoise
{
public:
	PerlinNoise();
	~PerlinNoise();

	template <class Real>
	static Real noise(Real x, Real y, Real z);

	template <class Real>
	static Real FBM(Real x, Real y, Real z, unsigned int num_octaves);

	static void init();

private:
	static int p[512];
};


#endif //__PERLINNOISE_H_666_
