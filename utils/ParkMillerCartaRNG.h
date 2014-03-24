/*=====================================================================
parkmillercartarng.h
--------------------
File created by ClassTemplate on Mon Oct 22 22:05:33 2007Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PARKMILLERCARTARNG_H_666_
#define __PARKMILLERCARTARNG_H_666_



#include "Platform.h"

/*=====================================================================
ParkMillerCartaRNG
------------------
From http://www.firstpr.com.au/dsp/rand31/rand31-park-miller-carta-int.c.txt
=====================================================================*/
class ParkMillerCartaRNG
{
public:
	/*=====================================================================
	ParkMillerCartaRNG
	------------------
	
	=====================================================================*/
	ParkMillerCartaRNG(uint32 seed);

	~ParkMillerCartaRNG();

	void seed(uint32 s);

	uint32 rand31pmc_ranlui();

	inline uint32 randUInt32();

private:
	uint32 seed31pmc;
};



uint32 ParkMillerCartaRNG::randUInt32()
{
	/*const uint32 top = (rand31pmc_ranlui() << 1) & 0xFFFF0000; // top 16 bits
	const uint32 bottom = (rand31pmc_ranlui() >> 15); // bottom 16 bits
	assert((top & 0x0000FFFF) == 0);
	assert((bottom & 0xFFFF0000) == 0);

	return top | bottom;*/

	return ((rand31pmc_ranlui() << 1) & 0xFFFF0000) // top 16 bits
		| (rand31pmc_ranlui() >> 15); // bottom 16 bits
}


#endif //__PARKMILLERCARTARNG_H_666_




