/*=====================================================================
VectorTest.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Tue May 25 16:47:39 +1200 2010
=====================================================================*/
#include "VectorTest.h"


#define VEC3_INLINE __forceinline


template <class Real>
class TestVec3
{
public:

	VEC3_INLINE TestVec3()
	{}

	VEC3_INLINE ~TestVec3()
	{}

	VEC3_INLINE TestVec3(Real x_, Real y_, Real z_)
	:	x(x_),
		y(y_),
		z(z_)
	{}

	VEC3_INLINE const TestVec3 operator * (Real factor) const
	{
		return TestVec3(x * factor, y * factor, z * factor);
	}

	VEC3_INLINE TestVec3& operator += (const TestVec3& rhs)
	{
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}

	Real x,y,z;
};


VectorTest::VectorTest()
{

}


VectorTest::~VectorTest()
{

}


//__declspec(nothrow)
static VEC3_INLINE TestVec3<float> testVMul(const TestVec3<float>& a, float f)
{
	//return Vec3f(a.x * f, a.y * f, a.z * f);
	TestVec3<float> res;
	res.x = a.x * f;
	res.y = a.y * f;
	res.z = a.z * f;
	return res;
}


float VectorTest::test()
{
	TestVec3<float> a(1.f, 2.f, 3.f);

	int N = 10000000;

	// Run a test using methods that return the vec3.
	{
		//Timer timer;
		TestVec3<float> c(0.f, 0.f, 0.f);
		for(int i=0; i<N; ++i)
		{
			const float factor = (float)i;
			const TestVec3<float> temp = testVMul(a, factor);
			c += temp;
		}

		//conPrint(c.toString());
		//conPrint(timer.elapsedString());
		return c.x;
	}

	
}
