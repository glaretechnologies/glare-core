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
#ifndef __MATHSTYPES_H__
#define __MATHSTYPES_H__


#include "Platform.h"
#include <cstring> // For std::memcpy()
#include <cmath>
#include <cassert>
#include <limits>


const double NICKMATHS_EPSILON = 0.00001;


namespace Maths
{


// Some template methods for getting Pi and various other values relating to Pi.
// It makes faster code to use the correct template specialisation, instead of always using e.g. the double precision value and casting to single precision.

template <class T> inline T pi();		// Pi
template <class T> inline T get2Pi();	// 2 * Pi
template <class T> inline T get4Pi();	// 4 * Pi

template <class T> inline T pi_2();		// Pi / 2
template <class T> inline T pi_4();		// Pi / 4

template <class T> inline T recipPi();	// 1 / Pi
template <class T> inline T recip2Pi(); // 1 / 2Pi
template <class T> inline T recip4Pi();	// 1 / 4Pi


template <> inline float  pi<float>()  { return 3.1415926535897932384626433832795f; };
template <> inline double pi<double>() { return 3.1415926535897932384626433832795; };

template <> inline float  get2Pi<float>()  { return 6.283185307179586476925286766559f; };
template <> inline double get2Pi<double>() { return 6.283185307179586476925286766559; };

template <> inline float  get4Pi<float>()  { return 12.566370614359172953850573533118f; };
template <> inline double get4Pi<double>() { return 12.566370614359172953850573533118; };


template <> inline float  pi_2<float>()  { return 1.5707963267948966192313216916398f; };
template <> inline double pi_2<double>() { return 1.5707963267948966192313216916398; };

template <> inline float  pi_4<float>()  { return 0.78539816339744830961566084581988f; };
template <> inline double pi_4<double>() { return 0.78539816339744830961566084581988; };


template <> inline float  recipPi<float>()  { return 0.31830988618379067153776752674503f; };
template <> inline double recipPi<double>() { return 0.31830988618379067153776752674503; };

template <> inline float  recip2Pi<float>()  { return 0.15915494309189533576888376337251f; };
template <> inline double recip2Pi<double>() { return 0.15915494309189533576888376337251; };

template <> inline float  recip4Pi<float>()  { return 0.07957747154594766788444188168626f; };
template <> inline double recip4Pi<double>() { return 0.07957747154594766788444188168626; };


} // end namespace Maths


const double NICKMATHS_PI = 3.1415926535897932384626433832795;

const double NICKMATHS_2PI = NICKMATHS_PI * 2.0;
const double NICKMATHS_4PI = NICKMATHS_PI * 4.0;

const double NICKMATHS_PI_2 = NICKMATHS_PI / 2.0;
const double NICKMATHS_PI_4 = NICKMATHS_PI / 4.0;

const double NICKMATHS_RECIP_PI = 1.0 / NICKMATHS_PI;
const double NICKMATHS_RECIP_2PI = 1.0 / NICKMATHS_2PI;
const double NICKMATHS_RECIP_4PI = 1.0 / NICKMATHS_4PI;

const float NICKMATHS_PIf = (float)NICKMATHS_PI;
const float NICKMATHS_2PIf = (float)NICKMATHS_2PI;
const float NICKMATHS_4PIf = (float)NICKMATHS_4PI;
const float NICKMATHS_PI_2f = (float)NICKMATHS_PI_2;
const float NICKMATHS_PI_4f = (float)NICKMATHS_PI_4;
const float NICKMATHS_RECIP_PIf = (float)(1.0 / NICKMATHS_PI);
const float NICKMATHS_RECIP_2PIf = (float)(1.0 / NICKMATHS_2PI);


// Natural log of 2
const double LN_2 = 0.69314718055994530941723212145818;


template <class Real>
inline bool epsEqual(Real a, Real b, Real epsilon = NICKMATHS_EPSILON)
{
	return fabs(a - b) <= epsilon;
}


template <class Real>
inline Real radToDegree(Real rad)
{
	return rad * 180 * Maths::recipPi<Real>();
	//return rad * (Real)180.0 / (Real)NICKMATHS_PI;
}


template <class Real>
inline Real degreeToRad(Real degree)
{
	return degree * Maths::pi<Real>() * (Real)0.00555555555555555555555555555556;
	//return degree * (Real)NICKMATHS_PI / (Real)180.0;
}



template <class T>
inline T myClamp(T x, T lowerbound, T upperbound)
{
	assert(lowerbound <= upperbound);

	/*if(x < lowerbound)
		return lowerbound;
	else if(x > upperbound)
		return upperbound;
	return x;*/
	return x < lowerbound ? lowerbound : (x > upperbound ? upperbound : x);
}


inline float absClamp(float x, float upperbound)
{
	if(fabs(x) <= upperbound)
		return x;

	return x < 0.0f ? -upperbound : upperbound;
}


template <class T>
inline const T myMin(const T x, const T y)
{
	return x <= y ? x : y;
}


template <class T>
inline const T myMin(const T x, const T y, const T z)
{
	return myMin(x, myMin(y, z));
}


//#include <type_traits> // Need C++11 support for this


template <class T>
inline const T myMax(const T x, const T y)
{
	// static_assert(std::is_pod<T>::value, "std::is_pod<T>::value");
	return x >= y ? x : y;
}


template <class T>
inline const T myMax(const T x, const T y, const T z)
{
	return myMax(x, myMax(y, z));
}


template <class T>
void mySwap(T& x, T& y)
{
	const T temp = x;
	x = y;
	y = temp;
}


//log
template <class Real>
inline Real logBase2(Real x)
{
	return log(x) / (Real)LN_2;
}


inline bool isNAN(float x)
{
#if defined(_WIN32)
	return _isnan(x) != 0;
#else
	return std::isnan(x) != 0;
#endif
}


inline bool isNAN(double x)
{
#if defined(_WIN32)
	return _isnan(x) != 0;
#else
	return std::isnan(x) != 0;
#endif
}


inline bool isFinite(float x)
{
	// If a floating point number is a NaN or INF, then all the exponent bits are 1.
	// So extract the exponent bits, and if they are less than the value which has 1 in all exponent bits (0x7f800000), then
	// this x is finite (not INF or NaN).
	// See http://www.h-schmidt.net/FloatConverter/

	// Use memcpy here to get around strict aliasing rules which disallow casting a float value to an int etc..
	// Compile should be able to 'see through' the memcpy and generate efficient code (confirmed in VS2012 x64 target)
	uint32 i;
	std::memcpy(&i, &x, 4);
	return (i & 0x7fffffff) < 0x7f800000;

	// Comparing use of _finite() with the code above:
	//
	// _finite() [float]
    //     cycles: 10.486554
    //     sum: 1000000
	// fastIsFinite() [float]    (code above)
    //     cycles: 4.863068999999999
    //     sum: 1000000

/*#if defined(_WIN32)
	return _finite(x) != 0;//hopefully this works for floats :)
#else
	return finite(x) != 0;//false;//TEMP HACK
#endif
	*/
}


inline bool isFinite(double x)
{
#if defined(_WIN32)
	return _finite(x) != 0;
#else
	return finite(x) != 0;//false;//TEMP HACK
#endif
}


inline bool isInf(float x)
{
	//return isNegInf(x) || isPosInf(x);
	return !isFinite(x) && !isNAN(x);
}


inline bool isInf(double x)
{
	//return isNegInf(x) || isPosInf(x);
	return !isFinite(x) && !isNAN(x);
}


inline bool isNegInf(float x)
{
	return isInf(x) && x < 0.0f;
}


inline bool isPosInf(float x)
{
	return isInf(x) && x > 0.0f;
}


inline bool isPosInf(double x)
{
	return isInf(x) && x > 0.0f;
}


/*inline bool isDenormed(float x)
{
	//denormed floats have a zeroed exponent field and a non-zero fractional field

	//TODO: test
	return (*(unsigned int*)(&x) & 0x7F800000) == 0 && (*(unsigned int*)(&x) & 0x7FFFFF) != 0;
}*/


//see http://mega-nerd.com/FPcast/
template <class Real>
inline int roundToInt(Real x)
{
	assert(x >= (Real)0.0);
#if defined(_WIN32) && !defined(_WIN64)
	return int(x + Real(0.5)); //NOTE: this is probably incorrect for negative numbers.
	/*int i;
	_asm
	{
		fld x		; Push x onto FP stack
		fistp i;	; convert to integer and store in i.
	}
	return i;*/
#elif defined(_WIN64)
	return int(x + Real(0.5)); //NOTE: this is probably incorrect for negative numbers.
#else
	//int i;
	//NOTE: testme
	//__asm__ __volatile__ ("fistpl %0" : "=m" (i) : "t" (x) : "st") ;
	//return i;

	return lrint(x);
#endif
}


template <class VecType>
inline typename VecType::RealType absDot(const VecType& v1, const VecType& v2)
{
	return std::fabs(dot(v1, v2));
}


namespace Maths
{

template <class Real>
inline bool approxEq(Real a, Real b, Real eps = (Real)NICKMATHS_EPSILON)
{
	if(a == 0.0 && b == 0.0)
		return true;
	return fabs(a - b) / fabs(a) <= eps;
}


template <class Real>
inline bool posUnderflowed(Real x)
{
	assert(x >= 0.0);

	// min returns the minimum normalised value for double.
	return x != 0.0 && x < std::numeric_limits<Real>::min();
}


template <class Real>
inline bool posOverflowed(Real x)
{
	assert(x >= 0.0);

	return x > std::numeric_limits<Real>::max();
}


// These are only correct for positive reals
inline int posFloorToInt(float x)
{
	assert(x >= 0.0f);
	return (int)x;
}


inline int posFloorToInt(double x)
{
	assert(x >= 0.0);
	return (int)x;
}


// NOTE: this may fail due to some floating point numbers not being expressible in ints
inline int floorToInt(float x)
{
	return (int)floor(x);
}


inline int floorToInt(double x)
{
	return (int)floor(x);
}


template <class Real>
inline Real fract(Real x)
{
	return x - std::floor(x);
}


const double SQRT_2PI = sqrt(NICKMATHS_2PI);
const double RECIP_SQRT_2PI = 1.0f / sqrt(NICKMATHS_2PI);


inline double eval1DGaussian(double x, double mean, double standard_dev)
{
	//return exp(-(x-mean)*(x-mean) / (2.0f*standard_dev*standard_dev)) / (standard_dev * sqrt(NICKMATHS_2PI));

	const double recip_standard_dev = 1.0 / standard_dev;

	return recip_standard_dev * RECIP_SQRT_2PI * exp(-0.5 * (x-mean)*(x-mean) * recip_standard_dev * recip_standard_dev);
}


inline double eval1DGaussian(double dist, double standard_dev)
{
	//return exp(-(x-mean)*(x-mean) / (2.0f*standard_dev*standard_dev)) / (standard_dev * sqrt(NICKMATHS_2PI));

	const double recip_standard_dev = 1.0 / standard_dev;

	return recip_standard_dev * RECIP_SQRT_2PI * exp(-0.5 * dist*dist * recip_standard_dev * recip_standard_dev);
}


//return positive solution
inline double inverse1DGaussian(double G, double standard_dev)
{
	return sqrt(-2.f * standard_dev * standard_dev * log(SQRT_2PI * standard_dev * G));
}


template <class Real>
inline Real eval2DGaussian(Real dist2, Real standard_dev)
{
	const Real recip_standard_dev_2 = 1 / (standard_dev*standard_dev);

	return Maths::recip2Pi<Real>() * recip_standard_dev_2 * std::exp((Real)-0.5 * dist2 * recip_standard_dev_2); 
}


inline double mitchellNetravali(double B, double C, double x)
{
	assert(x >= 0.0);

	if(x >= 2.0)
		return 0.0;
	else if(x >= 1.0)
	{
		return (1.0 / 6.0) * ((-B - 6.0*C)*x*x*x + (6.0*B + 30*C)*x*x + (-12.0*B - 48.0*C)*x + (8.0*B + 24.0*C));
	}
	else
	{
		return (1.0 / 6.0) * ((12.0 - 9.0*B - 6.0*C)*x*x*x + (-18.0 + 12.0*B + 6.0*C)*x*x + (6.0 - 2.0*B));
	}
}


// Returns B = C = 1/3 case
inline double mitchellNetravali(double x)
{
	assert(x >= 0.0);

	if(x >= 2.0)
		return 0.0;
	else if(x >= 1.0)
	{
		// 1.0 <= t < 2.0

		assert(epsEqual((-7.0 / 18.0)*x*x*x + 2.0*x*x - (20.0 / 6.0)*x + (32.0 / 18.0), mitchellNetravali(1.0/3.0, 1.0/3.0, x)));

		return (-7.0 / 18.0)*x*x*x + 2.0*x*x - (20.0 / 6.0)*x + (32.0 / 18.0);
	}
	else
	{
		// t < 1.0
		assert(epsEqual((7.0 / 6.0)*x*x*x - 2.0*x*x + (16.0 / 18.0), mitchellNetravali(1.0/3.0, 1.0/3.0, x)));

		return (7.0 / 6.0)*x*x*x - 2.0*x*x + (16.0 / 18.0);
	}
}


//inclusive
template <class T>
inline bool inRange(T x, T min, T max)
{
	return x >= min && x <= max;
}


template <class T>
inline bool inUnitInterval(T x)
{
	return x >= (T)0.0 && x <= (T)1.0;
}


template <class T>
inline bool inHalfClosedInterval(T x, T min, T max)
{
	return x >= min && x < max;
}


template <class T>
inline T square(T x)
{
	return x * x;
}


template <class T>
inline T pow3(T x)
{
	return x * x * x;
}


template <class T>
inline T pow4(T x)
{
	const T x2 = x*x;
	return x2 * x2;
}


template <class T>
inline T pow8(T x)
{
	const T x2 = x*x;
	const T x4 = x2*x2;
	return x4*x4;
}


template <class T>
inline T tanForCos(T cos_theta)
{
	assert(cos_theta >= (T)-1.0 && cos_theta <= (T)1.0);

	// sin(theta)^2 + cos(theta)^2 + 1
	// sin(theta) = sqrt(1 - cos(theta)^2)
	return sqrt((T)1.0 - cos_theta*cos_theta) / cos_theta;
}


// Note: this can overflow, and only works for x >= 0
template <class T>
inline T roundedUpDivide(T x, T N)
{
	assert(x >= 0);
	assert(N >  0);
	return (x + N - 1) / N;
}


// from http://en.wikipedia.org/wiki/Power_of_two#Fast_algorithm_to_check_if_a_number_is_a_power_of_two
template <class T>
inline bool isPowerOfTwo(T x)
{
	return (x > 0) && ((x & (x - 1)) == 0);
}


template <class T, class Real>
inline const T lerp(const T& a, const T& b, Real t)
{
	assert(Maths::inRange(t, (Real)0.0, (Real)1.0));
	return a * (1 - t) + b * t;
}


template <class T>
inline const T uncheckedLerp(const T& a, const T& b, float t)
{
	return a * (1 - t) + b * t;
}


template <class T>
inline const T uncheckedLerp(const T& a, const T& b, double t)
{
	return a * (1 - t) + b * t;
}


// From http://en.wikipedia.org/wiki/Smoothstep
template <class T>
inline T smoothStep(T a, T b, T x)
{
	// Scale, and clamp x to 0..1 range
	x = myClamp<T>((x - a)/(b - a), 0, 1);

	// Evaluate polynomial
	return x*x*x*(x*(x*6 - 15) + 10);
}


// Code for a fast, approximate, pow() function: fastPow()
// Runs in about 46 cycles on an Intel Core i5 as opposed to about 96 cycles for pow().
// NOTE: could be SIMD'd
// Adapted from 'Production Rendering', page 270.
inline float fastLog2(float a)
{
	// This code basically extracts the exponent in floating point form.
	union U
	{
		float f;
		int i;
	};
	U u;
	u.f = a;
	float x = (float)u.i;
	//float x = (float)*((int*)&a);
	x *= 0.00000011920928955078125f; // 2^-23
	x -= 127.0f;
	
	float y = x - std::floor(x);
	y = (y-y*y) * 0.346607f;
	return x + y;
}


inline float fastPow2(float i)
{
	float y = i - std::floor(i);
	y = (y-y*y) * 0.33971f;

	float x = i + 127.0f - y;
	x *= 8388608.0f; // 2^23
	
	union U
	{
		float f;
		int i;
	};
	U u;
	u.i = (int)x;
	x = u.f;
	//*(int*)&x = (int)x;
	return x;
}


inline float fastPow(float a, float b)
{
	return fastPow2(b*fastLog2(a));
}


/*
Evaluate 1 - cos(x) for x >= 0.

This can suffer from catastrophic cancellation when x is close to zero when evaluated directly.
Instead we can evaluate in a different way when x is close to zero:

cos(x) = 1 - x^2/2! + x^4/4! - x^6/6! + ...
so
1 - cos(x) = 1 - (1 - x^2/2! + x^4/4! - x^6/6! + ...)
           = x^2/2! - x^4/4! + x^6/6! - ...
*/
template <class Real>
inline Real oneMinusCosX(Real x)
{
	assert(x >= 0);

	if(x < (Real)0.3)
	{
		return x*x/2 - x*x*x*x/24 + x*x*x*x*x*x/720;
	}
	else
	{
		return 1 - std::cos(x);
	}
}



void test();


} // end namespace Maths


#endif //__MATHSTYPES_H__
