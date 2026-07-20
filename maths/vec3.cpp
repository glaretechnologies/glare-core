/*=====================================================================
vec3.h
------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "vec3.h"


#include "mathstypes.h"
#include "../indigo/globals.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include <stdio.h>
#include <string>


// Do explicit template instantiation
template const std::string Vec3<float >::toStringFullPrecision() const;
template const std::string Vec3<float >::toStringMaxNDecimalPlaces(int n) const;

template const std::string Vec3<double>::toStringFullPrecision() const;
template const std::string Vec3<double>::toStringMaxNDecimalPlaces(int n) const;


// template specialisations:

template <>
const std::string Vec3<float>::toString() const
{
	//const int num_dec_places = 2;
	//return "(" + floatToString(x, num_dec_places) + "," + floatToString(y, num_dec_places) 
	//	+ "," + floatToString(z, num_dec_places) + ")";
	return "(" + ::toString(x) + "," + ::toString(y) + "," + ::toString(z) + ")";
}


template <>
const std::string Vec3<double>::toString() const
{
	//const int num_dec_places = 2;
	//return "(" + floatToString(x, num_dec_places) + "," + floatToString(y, num_dec_places) 
	//	+ "," + floatToString(z, num_dec_places) + ")";
	return "(" + ::toString(x) + "," + ::toString(y) + "," + ::toString(z) + ")";
}


template <>
const std::string Vec3<int>::toString() const
{
	return "(" + ::toString(x) + "," + ::toString(y) + "," + ::toString(z) + ")";
}


template <class Real>
const std::string Vec3<Real>::toStringFullPrecision() const
{
	return "(" + doubleToString(x) + "," + doubleToString(y) + "," + doubleToString(z) + ")";
}


template <class Real>
const std::string Vec3<Real>::toStringMaxNDecimalPlaces(int n) const
{
	return "(" + ::doubleToStringMaxNDecimalPlaces(x, n) + ", " + ::doubleToStringMaxNDecimalPlaces(y, n) + ", " + ::doubleToStringMaxNDecimalPlaces(z, n) + ")";
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"


//__declspec(nothrow)
static GLARE_STRONG_INLINE Vec3f vmul(const Vec3f& a, float f)
{
	//return Vec3f(a.x * f, a.y * f, a.z * f);
	Vec3f res;
	res.x = a.x * f;
	res.y = a.y * f;
	res.z = a.z * f;
	return res;
}


inline void vmul2(const Vec3f& a, float f, Vec3f& out)
{
	//return Vec3f(a.x * f, a.y * f, a.z * f);
	Vec3f res;
	res.x = a.x * f;
	res.y = a.y * f;
	res.z = a.z * f;
	out = res;
}


inline void vadd(const Vec3f& a, const Vec3f& b, Vec3f& out)
{
	Vec3f res;
	res.x = a.x + b.x;
	res.y = a.y + b.y;
	res.z = a.z + b.z;
	out = res;
}


template <>
void Vec3<float>::test()
{
	{
		Vec3f v(10.f, 20.f, 30.f);
		testAssert(v.toVec4fVector() == Vec4f(10.f, 20.f, 30.f, 0));
		testAssert(v.toVec4fPoint()  == Vec4f(10.f, 20.f, 30.f, 1.f));
	}
	{
		Vec3d v(10, 20, 30);
		testAssert(v.toVec4fVector() == Vec4f(10.f, 20.f, 30.f, 0));
		testAssert(v.toVec4fPoint()  == Vec4f(10.f, 20.f, 30.f, 1.f));
	}


	//============================ Measure speed of toVec4fVector etc. =============================

	//----------------------------- Vec3f -------------------------------------
	{
		const int trials = 100;
		const int N = 1000;
		std::vector<Vec3f> data(N);
		for(int i=0; i<N; ++i)
			data[i] = Vec3f(10, 20, 30);

		{
			Timer timer;
			Vec4f sum(0.f);
			for(int t=0; t<trials; ++t)
			for(int i=0; i<N; ++i)
			{
				sum += data[i].toVec4fVector();
			}
			const double elapsed = timer.elapsed() / (N * trials);
			conPrint("\t\t\t\t\t Vec3f::toVec4fVector took       " + doubleToStringNDecimalPlaces(elapsed * 1.0e9) + " ns");
			printVar(sum);
		}
	}

	//----------------------------- Vec3d -------------------------------------
	{
		const int trials = 100;
		const int N = 1000;
		std::vector<Vec3d> data(N);
		for(int i=0; i<N; ++i)
			data[i] = Vec3d(10, 20, 30);

		{
			Timer timer;
			Vec4f sum(0.f);
			for(int t=0; t<trials; ++t)
			for(int i=0; i<N; ++i)
			{
				sum += data[i].toVec4fVector();
			}
			const double elapsed = timer.elapsed() / (N * trials);
			conPrint("\t\t\t\t\t Vec3d::toVec4fVector took       " + doubleToStringNDecimalPlaces(elapsed * 1.0e9) + " ns");
			printVar(sum);
		}
	}




	//__m128 temp_hakz = { 0.f };

	const Vec3f a(1.f, 2.f, 3.f);
	//const Vec3f b(4.f, 5.f, 6.f);
	int N = 10000000;

	// Run a test using methods that write to the 3rd parameter.
	{
		Timer timer;
		Vec3f c(0.f, 0.f, 0.f);
		for(int i=0; i<N; ++i)
		{
			const float factor = (float)i;
			Vec3f temp;
			vmul2(a, factor, temp);
			vadd(c, temp, c);
		}

		conPrint(c.toString());
		conPrint(timer.elapsedString());
	} 

	// Run a test using methods that return the vec3.
	{
		Timer timer;
		Vec3f c(0.f, 0.f, 0.f);
		for(int i=0; i<N; ++i)
		{
			const float factor = (float)i;
			const Vec3f temp = vmul(a, factor);
			c += temp;
		}

		conPrint(c.toString());
		conPrint(timer.elapsedString());
	}

	//conPrint(::toString(temp_hakz.m128_f32[0]));
}
#endif


/*const Vec3 Vec3::zerovector(0, 0, 0);
const Vec3 Vec3::i(1,0,0);	
const Vec3 Vec3::j(0,1,0);			
const Vec3 Vec3::k(0,0,1);*/

//Vec3 Vec3::ws_up(0,0,1);
//Vec3 Vec3::ws_right(0,-1,0);
//Vec3 Vec3::ws_forwards(1,0,0);


#if 0
const Vec3 Vec3::getAngles(const Vec3& ws_forwards, const Vec3& ws_up, const Vec3& ws_right) const
{
	const float ws_forwards_comp = this->dotProduct(ws_forwards);
	const float ws_right_comp = this->dotProduct(ws_right);
	const float ws_up_comp = this->dotProduct(ws_up);

	float theta;
	float phi;



	if(ws_forwards_comp == 0)
	{
		if(ws_right_comp > 0)
			theta = -NICKMATHS_PI_2;
		else
			theta = NICKMATHS_PI_2;
	}
	else 
	{
		if(ws_forwards_comp < 0)
			theta = NICKMATHS_PI + atan(-ws_right_comp / ws_forwards_comp);
		else
			theta = atan(-ws_right_comp / ws_forwards_comp);
	}






	//-----------------------------------------------------------------
	//calc theta
	//-----------------------------------------------------------------
	/*if(ws_right_comp == 0)
	{
		if(ws_forwards_comp > 0)
			theta = NICKMATHS_PI_2;
		else
			theta = -NICKMATHS_PI_2;
	}
	else 
	{
		if(ws_right_comp < 0)
			theta = NICKMATHS_PI + atan(ws_forwards_comp / ws_right_comp);
		else
			theta = atan(ws_forwards_comp / ws_right_comp);
	}*/	








	//-----------------------------------------------------------------
	//calc phi
	//-----------------------------------------------------------------
	const float xyproj_length = sqrt(ws_right_comp*ws_right_comp + ws_forwards_comp*ws_forwards_comp);

	if(xyproj_length == 0)
	{
		if(ws_up_comp > 0)
			phi = NICKMATHS_PI_2;
		else
			phi = -NICKMATHS_PI_2;
	}
	else
	{
		if(xyproj_length < 0)
			phi = NICKMATHS_PI + atan(ws_up_comp / xyproj_length);
		else
			phi = atan(ws_up_comp / xyproj_length);
	}



	return Vec3(theta, phi, 0);












	/*float veclength = length();

	if(veclength == 0)
	{
		return Vec3(0, 0, 0);
	}



	float ws_forwards_comp = this->dotProduct(ws_forwards);
	float ws_right_comp = this->dotProduct(ws_right);

	float yaw;

	if(ws_forwards_comp == 0)//if no forwards component
	{
		//yaw could be + Pi/2 or -Pi/2.

		if(ws_right_comp > 0)
		{	//if vector points to the right
			yaw = -NICKMATHS_PI / 2.0f;
		}
		else
		{
			yaw = NICKMATHS_PI / 2.0f;
		}
	}
	else
	{
		yaw = asin(ws_right_comp / ws_forwards_comp);
	}

	float ws_up_comp = this->dotProduct(ws_up);

	float pitch = asin(ws_up_comp / veclength);

	return Vec3(yaw, pitch, 0);*/
}
/*const Vec3 getAngles() const //around i, j, k
{
	float xrot;

	//-----------------------------------------------------------------
	//calc rot around i (x-axis)
	//-----------------------------------------------------------------
	if(y == 0)
	{
		if(z > 0)
			theta = NICKMATHS_PI_2;
		else
			theta = -NICKMATHS_PI_2;
	}
	else 
	{
		if(y < 0)
			theta = NICKMATHS_PI + atan(z / y);
		else
			theta = atan(z / y);
	}	



	float yrot;

	if(z == 0)//if no forwards component
	{
		//yaw could be + Pi/2 or -Pi/2.

		if(ws_right_comp > 0)
		{	//if vector points to the right
			yaw = -NICKMATHS_PI / 2.0f;
		}
		else
		{
			yaw = NICKMATHS_PI / 2.0f;
		}
	}
	else
	{
		yaw = asin(ws_right_comp / ws_forwards_comp);
	}

	float ws_up_comp = this->dotProduct(ws_up);

	float pitch = asin(ws_up_comp / veclength);

	return Vec3(yaw, pitch, 0);

}*/




const Vec3 Vec3::fromAngles(const Vec3& ws_forwards, const Vec3& ws_up, const Vec3& ws_right) const
{
	Vec3 out(0,0,0);
	out += ws_up * sin(x);
	out += ws_forwards * cos(y)*cos(x);
	out += ws_right * sin(y)*cos(x);

	return out;
}
	
	


const Vec3 Vec3::randomVec(float component_lowbound, float component_highbound)
{
	return Vec3(Random::inRange(component_lowbound, component_highbound),
				Random::inRange(component_lowbound, component_highbound),
				Random::inRange(component_lowbound, component_highbound));
}


#endif // BUILD_TESTS

