/*=====================================================================
SSE.cpp
-------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "SSE.h"


#include "../utils/Platform.h"
#include "../maths/mathstypes.h"
#include <assert.h>
#include <new>
#ifdef COMPILER_MSVC
#include <intrin.h>
#else
#include <stdlib.h>
#include <errno.h>
#endif


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../utils/CycleTimer.h"


void SSETest()
{
	conPrint("SSETest()");
	
	/*conPrint("\n====================Scalar max ===============");
	{
		CycleTimer cycle_timer;

		const float N = 100000;
		float sum = 0.f;

		SSE_ALIGN float fv[4] = { 1.f, 0, 20000.f, 0 };

		float f;
		for( f=0; f<N; f += 1.f)
		{
			fv[3] = f;

			sum += myMax(fv[0], myMax(fv[1], myMax(fv[2], fv[3])));
		}

		const uint64 cycles = cycle_timer.elapsed();
		printVar((float)cycles);
		printVar((float)cycles / N);
		printVar(sum);
	}*/


#if !defined(OSX)

	/*conPrint("\n==================== myAlignedMalloc vs _mm_malloc speed test ===============");
	{
		Timer timer;
		// Test myAlignedMalloc, myAlignedFree
		for(int i=0; i<1000; ++i)
		{
			for(int a=0; a<10; ++a)
			{
				void* m = SSE::alignedMalloc(i, 4 << a);

				// Write to the memory
				//for(int z=0; z<i; ++z)
				//{
				//	((unsigned char*)m)[z] = 0;
				//}

				SSE::alignedFree(m);
			}
		}

		conPrint("alignedMalloc(): " + timer.elapsedString());
	}
	{
		Timer timer;
		for(int i=0; i<1000; ++i)
		{
			for(int a=0; a<10; ++a)
			{
				void* m = _mm_malloc(i, 4 << a);

				// Write to the memory
				//for(int z=0; z<i; ++z)
				//{
				//	((unsigned char*)m)[z] = 0;
				//}

				_mm_free(m);
			}
		}

		conPrint("_mm_malloc: " + timer.elapsedString());
	}
	{
		Timer timer;
		// Test myAlignedMalloc, myAlignedFree
		for(int i=0; i<1000; ++i)
		{
			for(int a=0; a<10; ++a)
			{
				void* m = SSE::alignedMalloc(i, 4 << a);

				// Write to the memory
				//for(int z=0; z<i; ++z)
				//{
				//	((unsigned char*)m)[z] = 0;
				//}

				SSE::alignedFree(m);
			}
		}

		conPrint("alignedMalloc(): " + timer.elapsedString());
	}*/

	conPrint("\n====================Test SSE horizontal max ===============");
	//{
	//	CycleTimer cycle_timer;

	//	const float N = 100000;
	//	float sum = 0.f;

	//	//SSE_ALIGN float fv[4] = { 1.f, 0, 20000.f, 0 };

	//	float f;
	//	for( f=0; f<N; f += 1.f)
	//	{
	//		//fv[3] = f;
	//		//SSE_ALIGN float fv[4] = { f + 1.f, f, f + 2.f, f };

	//		//sum += sseGetMax(_mm_load_ps(fv));
	//		sum += horizontalMax(_mm_load_ps1(&f));
	//	}

	//	const uint64 cycles = cycle_timer.elapsed();
	//	printVar((float)cycles);
	//	printVar((float)cycles / N);
	//	printVar(sum);
	//}
#endif // !defined(OSX)

	{
		SSE_ALIGN float x[4] = { 1.f, 2.f, 3.f, 4.f };
		testAssert(horizontalMax(_mm_load_ps(x)) == 4.f);
	}
	{
		SSE_ALIGN float x[4] = { 5.f, 2.f, 3.f, 4.f };
		testAssert(horizontalMax(_mm_load_ps(x)) == 5.f);
	}
	{
		SSE_ALIGN float x[4] = { 5.f, 6.f, 3.f, 4.f };
		testAssert(horizontalMax(_mm_load_ps(x)) == 6.f);
	}
	{
		SSE_ALIGN float x[4] = { 5.f, 6.f, 7.f, 4.f };
		testAssert(horizontalMax(_mm_load_ps(x)) == 7.f);
	}

	conPrint("\n====================Test SSE horizontal min ===============");
	{
		SSE_ALIGN float x[4] = { 1.f, 2.f, 3.f, 4.f };
		testAssert(horizontalMin(_mm_load_ps(x)) == 1.f);
	}
	{
		SSE_ALIGN float x[4] = { 5.f, 2.f, 3.f, 4.f };
		testAssert(horizontalMin(_mm_load_ps(x)) == 2.f);
	}
	{
		SSE_ALIGN float x[4] = { 5.f, 6.f, 3.f, 4.f };
		testAssert(horizontalMin(_mm_load_ps(x)) == 3.f);
	}
	{
		SSE_ALIGN float x[4] = { 5.f, 6.f, 7.f, 4.f };
		testAssert(horizontalMin(_mm_load_ps(x)) == 4.f);
	}
}


#endif // BUILD_TESTS
