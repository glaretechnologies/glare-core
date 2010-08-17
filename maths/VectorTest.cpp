/*=====================================================================
VectorTest.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Tue May 25 16:47:39 +1200 2010
=====================================================================*/
#include "VectorTest.h"


//#include "../utils/timer.h"
//#include "../utils/stringutils.h"
//#include "../indigo/globals.h"


#if defined(WIN32) || defined(WIN64)
#define VEC3_INLINE __forceinline
#define NOTHROW __declspec(nothrow)
#else
#define VEC3_INLINE inline
#define NOTHROW
#endif


//template <class Real>
typedef float TVReal;
class TestVec3
{
public:

	NOTHROW VEC3_INLINE TestVec3() throw ()
	{}

	NOTHROW VEC3_INLINE TestVec3(const TestVec3& other) throw ()
	:	x(other.x),
		y(other.y),
		z(other.z)
	{}

	NOTHROW VEC3_INLINE ~TestVec3() throw ()
	{}

	NOTHROW VEC3_INLINE TestVec3(TVReal x_, TVReal y_, TVReal z_) throw ()
	:	x(x_),
		y(y_),
		z(z_)
	{}

	VEC3_INLINE const TestVec3 operator * (TVReal factor) const throw ()
	{
		return TestVec3(x * factor, y * factor, z * factor);
	}

	VEC3_INLINE TestVec3& operator += (const TestVec3& rhs) throw ()
	{
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}

	TVReal x,y,z;
};


VectorTest::VectorTest()
{

}


VectorTest::~VectorTest()
{

}


NOTHROW
static VEC3_INLINE const TestVec3 testVMul(const TestVec3& a, TVReal f) throw ()
{
	return TestVec3(a.x * f, a.y * f, a.z * f);
	/*TestVec3 res;
	res.x = a.x * f;
	res.y = a.y * f;
	res.z = a.z * f;
	return res;*/
}


float VectorTest::test()
{
	TestVec3 a(1.f, 2.f, 3.f);

	int N = 10000000;

	// Run a test using methods that return the vec3.
	{
		//Timer timer;
		TestVec3 c(0.f, 0.f, 0.f);
		for(int i=0; i<N; ++i)
		{
			const float factor = (float)i;
			const TestVec3 temp = testVMul(a, factor);
			c += temp;
		}

		/*conPrint(toString(c.x));
		conPrint(toString(c.y));
		conPrint(toString(c.z));
		conPrint(timer.elapsedString());*/
		return c.x;
	}

//	exit(0);
	
}
