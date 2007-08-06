/*===================================================================

  
  digital liberation front 2001
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|        \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#ifndef __RANDOM_H__
#define __RANDOM_H__


//#define random()	((rand () & 0x7fff) / ((float)0x7fff))
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <assert.h>

class Random
{
public:

	inline static unsigned char randByte()
	{
		return (unsigned char)((rand() & 0xFF0) >> 4);
	}

	inline static unsigned int randUnsignedInt()
	{
		return ((rand() & 0xFF0) << 20) | //leftmost byte
				((rand() & 0xFF0) << 12) | 
				((rand() & 0xFF0) << 4) | 
				(rand() & 0xFF0) >> 4;	//rightmost byte

		///also could use something like 
		//((rand()&0x7f80)<<17) &&((rand()&0x7f8)<<9)&&((rand()&0x7f80<<1)&&(rand()&0x7f80>>7)
	}

	inline static int randInt()
	{
		const unsigned int r = randUnsignedInt();

		return *((int*)&r);
	}

	inline static int randPositiveInt()
	{
		return (int)(randUnsignedInt() & 0x7FFFFFFF);//mask zeroes sign bit
	}

	inline static float inRange(float lower_bound, float upper_bound)
	{
		return lower_bound + (unit() * (upper_bound - lower_bound));
	}

	inline static float unit()
	{
		//return (rand() & 0x7fff) / (float)0x7fff;
		return (float)randUnsignedInt() / (float)0xFFFFFFFF;
	}

	
	inline static double unitDouble()
	{
		//return (rand() & 0x7fff) / (float)0x7fff;
		return (double)randUnsignedInt() / (double)0xFFFFFFFF;
	}


	//NOTE: get a proper gaussian
	/*inline static float gaussian()
	{
		float x = unit();

		if(x == 0)
			x = 0.000001f;

		const float y = (float)sqrt( -log(x));

		if(unit() > 0.5)
		{
			return -y;
		}
		else
		{
			return y;
		}
	}

	inline static float gaussianInRange(float lower_bound, float upper_bound, float multer)
	{
		float g = ((upper_bound - lower_bound) * 0.5f) + gaussian() * multer;

		if(g > upper_bound)
			g = upper_bound;
		else if(g < lower_bound)
			g = lower_bound;

		return g;
	}*/

	inline static void seedWithTime()
	{
		assert(RAND_MAX >= 0x7FFF);

		srand(static_cast<unsigned int>(time(NULL)));
	}


private:
	Random();
};


#endif //__RANDOM_H__
