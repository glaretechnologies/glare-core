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


#include "../utils/Platform.h"
#include "../utils/BitUtils.h"
#include <xmmintrin.h> // SSE header file
#include <emmintrin.h> // SSE 2 header file
#include <cstring> // For std::memcpy()
#include <cmath>
#include <cassert>
#include <limits>


const double NICKMATHS_EPSILON = 0.00001;


namespace Maths
{


// Some template methods for getting Pi and various other values relating to Pi.
// It makes faster code to use the correct template specialisation, instead of always using e.g. the double precision value and casting to single precision.
// Using a const double initialiser like 'const double NICKMATHS_RECIP_PI = 1.0 / NICKMATHS_PI;' also does the divide in *each* translation unit at program 
// initialisation time.


template <class T> INDIGO_STRONG_INLINE T pi();		// Pi
template <class T> INDIGO_STRONG_INLINE T get2Pi();	// 2 * Pi
template <class T> INDIGO_STRONG_INLINE T get4Pi();	// 4 * Pi

template <class T> INDIGO_STRONG_INLINE T pi_2();		// Pi / 2
template <class T> INDIGO_STRONG_INLINE T pi_4();		// Pi / 4

template <class T> INDIGO_STRONG_INLINE T recipPi();	// 1 / Pi
template <class T> INDIGO_STRONG_INLINE T recipPiSqd();	// 1 / Pi^2
template <class T> INDIGO_STRONG_INLINE T recip2Pi(); // 1 / 2Pi
template <class T> INDIGO_STRONG_INLINE T recip4Pi();	// 1 / 4Pi


template <> INDIGO_STRONG_INLINE float  pi<float>()  { return 3.1415926535897932384626433832795f; };
template <> INDIGO_STRONG_INLINE double pi<double>() { return 3.1415926535897932384626433832795; };

template <> INDIGO_STRONG_INLINE float  get2Pi<float>()  { return 6.283185307179586476925286766559f; };
template <> INDIGO_STRONG_INLINE double get2Pi<double>() { return 6.283185307179586476925286766559; };

template <> INDIGO_STRONG_INLINE float  get4Pi<float>()  { return 12.566370614359172953850573533118f; };
template <> INDIGO_STRONG_INLINE double get4Pi<double>() { return 12.566370614359172953850573533118; };


template <> INDIGO_STRONG_INLINE float  pi_2<float>()  { return 1.5707963267948966192313216916398f; };
template <> INDIGO_STRONG_INLINE double pi_2<double>() { return 1.5707963267948966192313216916398; };

template <> INDIGO_STRONG_INLINE float  pi_4<float>()  { return 0.78539816339744830961566084581988f; };
template <> INDIGO_STRONG_INLINE double pi_4<double>() { return 0.78539816339744830961566084581988; };


template <> INDIGO_STRONG_INLINE float  recipPi<float>()  { return 0.31830988618379067153776752674503f; };
template <> INDIGO_STRONG_INLINE double recipPi<double>() { return 0.31830988618379067153776752674503; };

template <> INDIGO_STRONG_INLINE float  recipPiSqd<float>()  { return 0.10132118364233777144387946320973f; };
template <> INDIGO_STRONG_INLINE double recipPiSqd<double>() { return 0.10132118364233777144387946320973; };

template <> INDIGO_STRONG_INLINE float  recip2Pi<float>()  { return 0.15915494309189533576888376337251f; };
template <> INDIGO_STRONG_INLINE double recip2Pi<double>() { return 0.15915494309189533576888376337251; };

template <> INDIGO_STRONG_INLINE float  recip4Pi<float>()  { return 0.07957747154594766788444188168626f; };
template <> INDIGO_STRONG_INLINE double recip4Pi<double>() { return 0.07957747154594766788444188168626; };


} // end namespace Maths


// TODO: remove these eventually.
const double NICKMATHS_PI = 3.1415926535897932384626433832795;

const double NICKMATHS_2PI = 6.283185307179586476925286766559;
const double NICKMATHS_4PI = 12.566370614359172953850573533118;

const double NICKMATHS_PI_2 = 1.5707963267948966192313216916398;
const double NICKMATHS_PI_4 = 0.78539816339744830961566084581988;

const double NICKMATHS_RECIP_PI = 0.31830988618379067153776752674503;
const double NICKMATHS_RECIP_2PI = 0.15915494309189533576888376337251;
const double NICKMATHS_RECIP_4PI = 0.07957747154594766788444188168626;

const float NICKMATHS_PIf = 3.1415926535897932384626433832795f;
const float NICKMATHS_2PIf = 6.283185307179586476925286766559f;
const float NICKMATHS_4PIf = 12.566370614359172953850573533118f;
const float NICKMATHS_PI_2f = 1.5707963267948966192313216916398f;
const float NICKMATHS_PI_4f = 0.78539816339744830961566084581988f;
const float NICKMATHS_RECIP_PIf = 0.31830988618379067153776752674503f;
const float NICKMATHS_RECIP_2PIf = 0.15915494309189533576888376337251f;


template <class Real>
INDIGO_STRONG_INLINE Real radToDegree(Real rad)
{
	return rad * (180 * Maths::recipPi<Real>());
}


template <class Real>
INDIGO_STRONG_INLINE Real degreeToRad(Real degree)
{
	return degree * (Maths::pi<Real>() * (Real)0.00555555555555555555555555555556);
}


template <class T>
INDIGO_STRONG_INLINE T myClamp(T x, T lowerbound, T upperbound)
{
	assert(lowerbound <= upperbound);

	return x < lowerbound ? lowerbound : (x > upperbound ? upperbound : x);
}


INDIGO_STRONG_INLINE float absClamp(float x, float upperbound)
{
	if(fabs(x) <= upperbound)
		return x;

	return x < 0.0f ? -upperbound : upperbound;
}


template <class T>
INDIGO_STRONG_INLINE const T myMin(const T x, const T y)
{
	return x <= y ? x : y;
}


template <class T>
INDIGO_STRONG_INLINE const T myMin(const T x, const T y, const T z)
{
	return myMin(x, myMin(y, z));
}


//#include <type_traits> // Need C++11 support for this


template <class T>
INDIGO_STRONG_INLINE const T myMax(const T x, const T y)
{
	// static_assert(std::is_pod<T>::value, "std::is_pod<T>::value");
	return x >= y ? x : y;
}


template <class T>
INDIGO_STRONG_INLINE const T myMax(const T x, const T y, const T z)
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


template <class Real>
INDIGO_STRONG_INLINE Real logBase2(Real x)
{
	return log(x) * (Real)1.4426950408889634073599246810019;
	// 1.4426950408889634073599246810019 = 1 / ln(2)
}


// Undefined if x == 0
INDIGO_STRONG_INLINE uint32 intLogBase2(uint64 x)
{
	assert(x != 0);
	return BitUtils::highestSetBitIndex(x);
}


INDIGO_STRONG_INLINE bool isNAN(float x)
{
	// _mm_cmpord_ss will set elem 0 to 0 if x is NaN, and 0xFFFFFFFF otherwise.
	__m128 vx = _mm_set_ss(x);
	return _mm_movemask_ps(_mm_cmpord_ss(vx, vx)) == 0x0;
}


INDIGO_STRONG_INLINE bool isNAN(double x)
{
	// _mm_cmpord_sd will set elem 0 to 0 if x is NaN, and 0xFFFFFFFFFFFFFFFF otherwise.
	__m128d vx = _mm_set_sd(x);
	return _mm_movemask_pd(_mm_cmpord_sd(vx, vx)) == 0x0;
}


INDIGO_STRONG_INLINE bool isFinite(float x)
{
	// If a floating point number is a NaN or INF, then all the exponent bits are 1.
	// So extract the exponent bits, and if they are less than the value which has 1 in all exponent bits (0x7f800000), then
	// this x is finite (not INF or NaN).
	// See http://www.h-schmidt.net/FloatConverter/

	// Use memcpy here to get around strict aliasing rules which disallow casting a float value to an int etc..
	// Compile should be able to 'see through' the memcpy and generate efficient code (confirmed in VS2012 x64 target)
	uint32 i;
	std::memcpy(&i, &x, 4);
	return (i & 0x7f800000) < 0x7f800000;

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


INDIGO_STRONG_INLINE bool isFinite(double x)
{
#if defined(_WIN32)
	return _finite(x) != 0;
#else
	return std::isfinite(x) != 0;
#endif
}


INDIGO_STRONG_INLINE bool isInf(float x)
{
	return !isFinite(x) && !isNAN(x);
}


INDIGO_STRONG_INLINE bool isInf(double x)
{
	return !isFinite(x) && !isNAN(x);
}


/*inline bool isDenormed(float x)
{
	//denormed floats have a zeroed exponent field and a non-zero fractional field

	//TODO: test
	return (*(unsigned int*)(&x) & 0x7F800000) == 0 && (*(unsigned int*)(&x) & 0x7FFFFF) != 0;
}*/


template <class Real>
inline bool epsEqual(Real a, Real b, Real epsilon = NICKMATHS_EPSILON)
{
	// With fast-math (/fp:fast) enabled, comparisons are not checked for NAN compares (the 'unordered predicate').
	// So we need to do this explicitly ourselves.
	const Real fabs_diff = fabs(a - b);
	if(isNAN(fabs_diff))
		return false;
	return fabs_diff <= epsilon;
}


//see http://mega-nerd.com/FPcast/
template <class Real>
INDIGO_STRONG_INLINE int roundToInt(Real x)
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
INDIGO_STRONG_INLINE typename VecType::RealType absDot(const VecType& v1, const VecType& v2)
{
	return std::fabs(dot(v1, v2));
}


namespace Maths
{


// Multiply by this constant to convert a unsigned 32 bit integer to a float in [0, 1).
// 1 / 2^32 = 2^-32 = 2.3283064365386963e-10, which is exactly representable as a float. (exponent=2^-32, mantissa=1)
// Lets consider the largest integer in [0, 2^32): 4294967295.
// 4294967295 * (2^-32) = 0.9999999997671694, which when rounded to the nearest float, gives 1.0f.
// Let's say we want to multiply by x, such that:
// 4294967295 * x = 0.99999994f			(0.99999994f is the largest float value below 1.0f)
// Then x = 0.99999994 / 4294967295 = 2.3283062973824113e-10
// or rounded to the nearest float: 2.3283063E-10.
// So we will multiply our uint32s by 2.3283063E-10f instead of 2^-32.
// Note that 2.3283063E-10f is the next float value below 2^-32, so it's the best we can do in terms of reducing bias/distortion.
inline float uInt32ToUnitFloatScale() { return 2.3283063E-10f; }


template <class Real>
inline bool approxEq(Real a, Real b, Real eps = (Real)NICKMATHS_EPSILON)
{
	if(a == 0.0 && b == 0.0)
		return true;
	return fabs((a - b) / a) <= eps;
}


template <class Real>
INDIGO_STRONG_INLINE bool posUnderflowed(Real x)
{
	assert(x >= 0.0);

	// min returns the minimum normalised value for double.
	return x != 0.0 && x < std::numeric_limits<Real>::min();
}


template <class Real>
INDIGO_STRONG_INLINE bool posOverflowed(Real x)
{
	assert(x >= 0.0);

	return x > std::numeric_limits<Real>::max();
}


// These are only correct for positive reals
INDIGO_STRONG_INLINE int posFloorToInt(float x)
{
	assert(x >= 0.0f);
	return (int)x;
}


INDIGO_STRONG_INLINE int posFloorToInt(double x)
{
	assert(x >= 0.0);
	return (int)x;
}


// NOTE: this may fail due to some floating point numbers not being expressible in ints
INDIGO_STRONG_INLINE int floorToInt(float x)
{
	return (int)floor(x);
}


INDIGO_STRONG_INLINE int floorToInt(double x)
{
	return (int)floor(x);
}


template <class Real>
INDIGO_STRONG_INLINE Real fract(Real x)
{
	return x - std::floor(x);
}


template <class T> INDIGO_STRONG_INLINE T sqrt2Pi();	// sqrt(2 Pi)
template <> INDIGO_STRONG_INLINE float  sqrt2Pi<float>()  { return 2.506628274631000502415765284811f; };
template <> INDIGO_STRONG_INLINE double sqrt2Pi<double>() { return 2.506628274631000502415765284811; };

template <class T> INDIGO_STRONG_INLINE T recipSqrt2Pi();	// 1 / sqrt(2 Pi)
template <> INDIGO_STRONG_INLINE float  recipSqrt2Pi<float>()  { return 0.39894228040143267793994605993438f; };
template <> INDIGO_STRONG_INLINE double recipSqrt2Pi<double>() { return 0.39894228040143267793994605993438; };


inline double eval1DGaussian(double x, double mean, double standard_dev)
{
	const double recip_standard_dev = 1.0 / standard_dev;

	return recip_standard_dev * recipSqrt2Pi<double>() * exp(-0.5 * (x-mean)*(x-mean) * recip_standard_dev * recip_standard_dev);
}


inline double eval1DGaussian(double dist, double standard_dev)
{
	const double recip_standard_dev = 1.0 / standard_dev;

	return recip_standard_dev * recipSqrt2Pi<double>() * exp(-0.5 * dist*dist * recip_standard_dev * recip_standard_dev);
}


// Return positive solution
inline double inverse1DGaussian(double G, double standard_dev)
{
	return sqrt(-2 * standard_dev * standard_dev * log(sqrt2Pi<double>() * standard_dev * G));
}


template <class Real>
inline Real eval2DGaussian(Real dist2, Real standard_dev)
{
	const Real recip_standard_dev_2 = 1 / (standard_dev*standard_dev);

	return Maths::recip2Pi<Real>() * recip_standard_dev_2 * std::exp((Real)-0.5 * dist2 * recip_standard_dev_2); 
}


// Inclusive
template <class T>
INDIGO_STRONG_INLINE bool inRange(T x, T min, T max)
{
	return x >= min && x <= max;
}


template <class T>
INDIGO_STRONG_INLINE bool inUnitInterval(T x)
{
	return x >= (T)0.0 && x <= (T)1.0;
}


template <class T>
INDIGO_STRONG_INLINE bool inHalfClosedInterval(T x, T min, T max)
{
	return x >= min && x < max;
}


template <class T>
INDIGO_STRONG_INLINE T square(T x)
{
	return x * x;
}


template <class T>
INDIGO_STRONG_INLINE T pow3(T x)
{
	return x * x * x;
}


template <class T>
INDIGO_STRONG_INLINE T pow4(T x)
{
	const T x2 = x*x;
	return x2 * x2;
}


template <class T>
INDIGO_STRONG_INLINE T pow6(T x)
{
	const T x2 = x*x;
	return x2 * x2 * x2;
}


template <class T>
INDIGO_STRONG_INLINE T pow8(T x)
{
	const T x2 = x*x;
	const T x4 = x2*x2;
	return x4*x4;
}


template <class T>
INDIGO_STRONG_INLINE T tanForCos(T cos_theta)
{
	assert(cos_theta >= (T)-1.0 && cos_theta <= (T)1.0);

	// sin(theta)^2 + cos(theta)^2 + 1
	// sin(theta) = sqrt(1 - cos(theta)^2)
	return sqrt((T)1.0 - cos_theta*cos_theta) / cos_theta;
}


// Note: this can overflow, and only works for x >= 0
template <class T>
INDIGO_STRONG_INLINE T roundedUpDivide(T x, T N)
{
	assert(x >= 0);
	assert(N >  0);
	return (x + N - 1) / N;
}


// Check that an integer is a power of two.
// from http://en.wikipedia.org/wiki/Power_of_two#Fast_algorithm_to_check_if_a_number_is_a_power_of_two
template <class T>
INDIGO_STRONG_INLINE bool isPowerOfTwo(T x)
{
	return (x > 0) && ((x & (x - 1)) == 0);
}


// See https://fgiesen.wordpress.com/2016/10/26/rounding-up-to-the-nearest-int-k-mod-n/
template <class T>
INDIGO_STRONG_INLINE T roundUpToMultipleOfPowerOf2(T x, T N)
{
	assert(x >= 0);
	assert(N > 0 && isPowerOfTwo(N));
	return (x + N - 1) & ~(N - 1);
}


// Adapted from http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
// Not correct for 0 input: returns 0 in this case.
inline uint64 roundToNextHighestPowerOf2(uint64 v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;
	return v + 1;
}


template <class T, class Real>
INDIGO_STRONG_INLINE const T lerp(const T& a, const T& b, Real t)
{
	assert(Maths::inRange(t, (Real)0.0, (Real)1.0));
	return a * (1 - t) + b * t;
}


template <class T>
INDIGO_STRONG_INLINE const T uncheckedLerp(const T& a, const T& b, float t)
{
	return a * (1 - t) + b * t;
}


template <class T>
INDIGO_STRONG_INLINE const T uncheckedLerp(const T& a, const T& b, double t)
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


// Euclidean modulo: result will be in [0, y) for positive y.
// e.g.
// floatMod(-3.f, 4.f) = 1.f
// floatMod(-2.f, 4.f) = 2.f
// ...
inline float floatMod(float x, float y)
{
	if(x < 0)
	{
		const float z = y - std::fmod(-x, y); // This will return a number in [0, y]
		return z == y ? 0.f : z;
	}
	else
		return std::fmod(x, y);
}


inline double doubleMod(double x, double y)
{
	if(x < 0)
	{
		const double z = y - std::fmod(-x, y); // This will return a number in [0, y]
		return z == y ? 0.0 : z;
	}
	else
		return std::fmod(x, y);
}


// Euclidean modulo: result will be in {0, 1, ...y-1} for positive y.
// e.g.
// intMod(-3, 4) = 1
// intMod(-2, 4) = 2
// ...
inline int intMod(int x, int y)
{
	const int z = x % y;
	return (z < 0) ? z + y : z;
}


// Return x with the sign of y.
// NOTE: these copySign functions are available in cmath as copysignf etc..,
// but we will define our own ones so they can be inlined.
inline float copySign(float x, float y)
{
	// Isolate sign bit of y
	uint32 signbit = bitCast<uint32>(y) & 0x80000000;

	// Zero sign bit of x and then set it to signbit.
	return bitCast<float>((bitCast<uint32>(x) & 0x7FFFFFFF) | signbit);
}


// Return x with the sign of y.
inline double copySign(double x, double y)
{
	// Isolate sign bit of y
	uint64 signbit = bitCast<uint64>(y) & 0x8000000000000000ULL;

	// Zero sign bit of x and then set it to signbit.
	return bitCast<double>((bitCast<uint64>(x) & 0x7FFFFFFFFFFFFFFFULL) | signbit);
}


// If x < 0, then return -1.f
// If x > 0, then return 1.f
// If x == +0, then return +0.0f
// If x == -0, then return -0.0f
inline float sign(float x)
{
	const float mag = (x == 0.f) ? 0.f : 1.f;
	return copySign(mag, x);
}


// If x < 0, then return -1.0
// If x > 0, then return 1.0
// If x == +0, then return +0.0
// If x == -0, then return -0.0
inline double sign(double x)
{
	const double mag = (x == 0.0) ? 0.0 : 1.0;
	return copySign(mag, x);
}


void test();


} // end namespace Maths


#endif //__MATHSTYPES_H__
