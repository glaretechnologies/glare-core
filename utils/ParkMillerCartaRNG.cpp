/*=====================================================================
parkmillercartarng.cpp
----------------------
File created by ClassTemplate on Mon Oct 22 22:05:33 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "ParkMillerCartaRNG.h"


#include <assert.h>

ParkMillerCartaRNG::ParkMillerCartaRNG(uint32 s)
{
	seed(s);
}


ParkMillerCartaRNG::~ParkMillerCartaRNG()
{
	    
}


void ParkMillerCartaRNG::seed(uint32 s)
{
	assert(s < 0x7FFFFFFE);
	seed31pmc = s + 1; // Can't be zero
}


#define constapmc 16807

uint32 ParkMillerCartaRNG::rand31pmc_ranlui()
{
	//long unsigned int hi, lo;
	//assert(sizeof(long unsigned int) == 4);
                                 
    uint32 lo = constapmc * (seed31pmc & 0xFFFF);
    const uint32 hi = constapmc * (seed31pmc >> 16);
    lo += (hi & 0x7FFF) << 16;
    lo += hi >> 15;                  
    if (lo > 0x7FFFFFFF) lo -= 0x7FFFFFFF;          
        
                                    /* lo contains the new pseudo-random
                                     * number.  Store it to the seed31 and
                                     * return it.
                                     */
        
    return ( seed31pmc = (long)lo ); 
}




