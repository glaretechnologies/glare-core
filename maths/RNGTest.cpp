/*=====================================================================
RNGTest.cpp
-----------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#include "RNGTest.h"


#if BUILD_TESTS


#include "../maths/PCG32.h"
//#include "MTwister.h"

#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../indigo/globals.h"
#include "Vec4f.h"


void RNGTest::test()
{
	const int N = 1000000;
	// Test construction/initialisation speed
	{
		/*{
			Timer timer;
			uint64 sum = 0;
			for(int i=0; i<N; ++i)
			{
				MTwister rng(i);
				//for(int z=0; z<600; ++z)
				//	sum += rng.genrand_int32();
			}

			const double elapsed = timer.elapsed();
			conPrint(toString(sum));
			conPrint("MTwister initialisation: " + toString(1.0e9 * elapsed / N) + " ns");
		}*/

		{
			Timer timer;
			uint64 sum = 0;
			for(int i=0; i<N; ++i)
			{
				PCG32 rng(1);
				//for(int z=0; z<600; ++z)
				//	sum += rng.nextUInt();
			}

			const double elapsed = timer.elapsed();
			conPrint(toString(sum));
			conPrint("PCG32 initialisation:    " + toString(1.0e9 * elapsed / N) + " ns");
		}
	}

	// Test speed of generating next random uint32.
	{
		/*{
			Timer timer;
			uint64 sum = 0;
			MTwister rng(1);
			for(int i=0; i<N; ++i)
			{
				sum += rng.genrand_int32();
			}

			const double elapsed = timer.elapsed();
			conPrint(toString(sum));
			conPrint("MTwister.genrand_int32(): " + toString(1.0e9 * elapsed / N) + " ns");
		}*/

		{
			Timer timer;
			uint64 sum = 0;
			PCG32 rng(1);
			for(int i=0; i<N; ++i)
			{
				sum += rng.nextUInt();
			}

			const double elapsed = timer.elapsed();
			conPrint(toString(sum));
			conPrint("PCG32.nextUInt():         " + toString(1.0e9 * elapsed / N) + " ns");
		}
	}

	// Test speed of generating next random float.
	{
		/*{
			Timer timer;
			float sum = 0;
			MTwister rng(1);
			for(int i=0; i<N; ++i)
			{
				sum = rng.unitRandom();
			}

			const double elapsed = timer.elapsed();
			conPrint(toString(sum));
			conPrint("MTwister.unitRandom():     " + toString(1.0e9 * elapsed / N) + " ns");
		}*/

		{
			Timer timer;
			float sum = 0;
			PCG32 rng(1);
			for(int i=0; i<N; ++i)
			{
				sum = rng.unitRandom();
			}

			const double elapsed = timer.elapsed();
			conPrint(toString(sum));
			conPrint("PCG32.unitRandom():        " + toString(1.0e9 * elapsed / N) + " ns");
		}
		

		{
			Timer timer;
			float sum = 0;
			PCG32 rng(1);
			for(int i=0; i<N; ++i)
			{
				sum = rng.unitRandomWithMul();
			}

			const double elapsed = timer.elapsed();
			conPrint(toString(sum));
			conPrint("PCG32.unitRandomWithMul(): " + toString(1.0e9 * elapsed / N) + " ns");
		}
	}
}


#endif
