/*=====================================================================
MTwister.h
----------
File created by ClassTemplate on Sun Jul 02 03:29:07 2006
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "../maths/mathstypes.h"


/*=====================================================================
MTwister
--------

=====================================================================*/
class MTwister
{
public:
	/*=====================================================================
	MTwister
	--------
	
	=====================================================================*/
	MTwister(unsigned long seed);

	~MTwister();

	void init_genrand(unsigned long s);

	// Returns a random float in [0, 1)
	inline float unitRandom() { return genrand_int32() * Maths::uInt32ToUnitFloatScale(); }
	unsigned long genrand_int32();

private:

	static const unsigned int N = 624;
	static const unsigned int M = 397;

	unsigned long mt[N]; /* the array for the state vector  */
	int mti; /* mti==N+1 means mt[N] is not initialized */
};
