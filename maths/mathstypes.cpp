#include "mathstypes.h"

#include "../indigo/TestUtils.h"

void Maths::test()
{
	testAssert(true);
	testAssert(roundToInt(0.0) == 0);
	testAssert(roundToInt(0.1) == 0);
	testAssert(roundToInt(0.5) == 0);
	testAssert(roundToInt(0.51) == 1);
	testAssert(roundToInt(0.9) == 1);
	testAssert(roundToInt(1.0) == 1);
	testAssert(roundToInt(1.1) == 1);
	testAssert(roundToInt(1.9) == 2);


	double one = 1.0;
	double zero = 0.0;
	testAssert(posOverflowed(one / zero));
	testAssert(!posOverflowed(one / 0.000000001));

	testAssert(posUnderflowed(1.0e-320));
	testAssert(!posUnderflowed(1.0e-300));
	testAssert(!posUnderflowed(1.0));
	testAssert(!posUnderflowed(0.0));

	testAssert(floorToInt(0.5) == 0);
	testAssert(floorToInt(0.0) == 0);
	testAssert(floorToInt(1.0) == 1);
	testAssert(floorToInt(1.1) == 1);
	testAssert(floorToInt(1.99999) == 1);
	testAssert(floorToInt(2.0) == 2);

	testAssert(epsEqual(tanForCos(0.2), tan(acos(0.2))));


	testAssert(!isPowerOfTwo((int)-4));
	testAssert(!isPowerOfTwo((int)-3));
	testAssert(!isPowerOfTwo((int)-2));
	testAssert(!isPowerOfTwo((int)-1));
	testAssert(!isPowerOfTwo((int)0));
	testAssert(!isPowerOfTwo((unsigned int)-3));
	testAssert(!isPowerOfTwo((unsigned int)-2));
	testAssert(!isPowerOfTwo((unsigned int)-1));
	testAssert(!isPowerOfTwo((unsigned int)0));

	testAssert(isPowerOfTwo((int)1));
	testAssert(isPowerOfTwo((int)2));
	testAssert(isPowerOfTwo((int)4));
	testAssert(isPowerOfTwo((int)65536));
	testAssert(isPowerOfTwo((unsigned int)1));
	testAssert(isPowerOfTwo((unsigned int)2));
	testAssert(isPowerOfTwo((unsigned int)4));
	testAssert(isPowerOfTwo((unsigned int)65536));

	testAssert(!isPowerOfTwo((int)3));
	testAssert(!isPowerOfTwo((int)9));
	testAssert(!isPowerOfTwo((unsigned int)3));
	testAssert(!isPowerOfTwo((unsigned int)9));
}
