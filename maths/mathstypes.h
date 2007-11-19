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

#include <math.h>
#include <float.h>
#include <assert.h>
#include <limits>

const double NICKMATHS_EPSILON = 0.00001;

const double NICKMATHS_PI = 3.1415926535897932384626433832795;

const double NICKMATHS_2PI = NICKMATHS_PI * 2.0;
const double NICKMATHS_4PI = NICKMATHS_PI * 4.0;

const double NICKMATHS_PI_2 = NICKMATHS_PI / 2.0;
const double NICKMATHS_PI_4 = NICKMATHS_PI / 4.0;

const double NICKMATHS_RECIP_PI = 1.0 / NICKMATHS_PI;
const double NICKMATHS_RECIP_2PI = 1.0 / NICKMATHS_2PI;
const double NICKMATHS_RECIP_4PI = 1.0 / NICKMATHS_4PI;

//natural log of 2
const double LN_2 = 0.69314718055994530941723212145818;


template <class Real>
inline bool epsEqual(Real a, Real b, Real epsilon = NICKMATHS_EPSILON)
{
	return fabs(a - b) <= epsilon;
}

template <class Real>
inline Real radToDegree(Real rad)
{
	return rad * (Real)180.0 / (Real)NICKMATHS_PI;	
}

template <class Real>
inline Real degreeToRad(Real degree)
{
	return degree * (Real)NICKMATHS_PI / (Real)180.0;
}

/*template <class Real>
inline Real absoluteVal(Real x)
{
	if(x < 0.0f)
		return -x;
	else
		return x;
}*/


inline float raiseBy2toN(float x, int n)
{
	//-----------------------------------------------------------------
	//isolate exponent bits
	//-----------------------------------------------------------------
	unsigned int exponent_bits = *((int*)&x) & 0x7F400000;

	//-----------------------------------------------------------------
	//bitshift them
	//-----------------------------------------------------------------
	exponent_bits >>= n;

	//-----------------------------------------------------------------
	//blank out old bits
	//-----------------------------------------------------------------
	*((int*)&x) &= 0x8001FFFF;

	//-----------------------------------------------------------------
	//copy new bits in
	//-----------------------------------------------------------------
	*((int*)&x) &= exponent_bits;

	return x;
}





// Fast reciprocal square root
//posted by DarkWIng on Flipcode

__inline float RSqrt( float number ) 
{
	long i;
	float x2, y;
	const float threehalfs = 1.5f;

	x2 = number * 0.5f;
	y  = number;
	i  = * (long *) &y;			// evil floating point bit level hacking
	i  = 0x5f3759df - (i >> 1);             // what the f..k?
	y  = * (float *) &i;
	y  = y * (threehalfs - (x2 * y * y));   // 1st iteration

	return y;
}

template <class T>
inline T myClamp(T x, T lowerbound, T upperbound)
{
	assert(lowerbound <= upperbound);

	if(x < lowerbound)
		return lowerbound;
	else if(x > upperbound)
		return upperbound;
	return x;
}

inline float absClamp(float x, float upperbound)
{
	if(fabs(x) <= upperbound)
		return x;

	return x < 0.0f ? -upperbound : upperbound;
}



template <class T>
inline const T& myMin(const T& x, const T& y)
{
	return x <= y ? x : y;
}

template <class T>
inline const T& myMax(const T& x, const T& y)
{
	return x >= y ? x : y;
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
#if defined(WIN32) || defined(WIN64)
	return _isnan(x) != 0;
#else
	return isnan(x) != 0;
#endif
}

inline bool isNAN(double x)
{
#if defined(WIN32) || defined(WIN64)
	return _isnan(x) != 0;
#else
	return isnan(x) != 0;
#endif
}

inline bool isFinite(float x)
{
#if defined(WIN32) || defined(WIN64)
	return _finite(x) != 0;//hopefully this works for floats :)
#else
	return finite(x) != 0;//false;//TEMP HACK
#endif
}

inline bool isFinite(double x)
{
#if defined(WIN32) || defined(WIN64)
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

inline bool isDenormed(float x)
{
	//denormed floats have a zeroed exponent field and a non-zero fractional field

	//TODO: test
	return (*(unsigned int*)(&x) & 0x7F800000) == 0 && (*(unsigned int*)(&x) & 0x7FFFFF) != 0;
}


//see http://mega-nerd.com/FPcast/
template <class Real>
inline int roundToInt(Real x)
{	
	assert(x >= (Real)0.0);	
#if defined(WIN32)
	int i;
	_asm
	{         
		fld x		; Push x onto FP stack
		fistp i;	; convert to integer and store in i.
	}
	return i;
#elif defined(WIN64)
	return int(x + Real(0.5)); //NOTE: this is probably incorrect for negative numbers.
#else
	//int i;
	//NOTE: testme
	//__asm__ __volatile__ ("fistpl %0" : "=m" (i) : "t" (x) : "st") ;
	//return i;

	return lrint(x);
#endif
}



/*
inline int roundToInt(double x)
{
	int i;
	_asm
	{         
		fld x;
		fistp i;
	}
	return i;
}
*/

//From http://homepages.inf.ed.ac.uk/rbf/HIPR2/gsmooth.htm

namespace Maths
{

inline bool posUnderflowed(double x)
{
	assert(x >= 0.0);

	//min returns the minimum normalised value for double.
	return x != 0.0 && x < std::numeric_limits<double>::min();
}
inline bool posOverflowed(double x)
{
	assert(x >= 0.0);

	return x > std::numeric_limits<double>::max();
}


inline int floorToInt(float x)
{
	return (int)x;
}
inline int floorToInt(double x)
{
	return (int)x;
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

inline double eval2DGaussian(double dist2, double standard_dev)
{
	const double recip_standard_dev_2 = 1.0 / (standard_dev*standard_dev);

	return NICKMATHS_RECIP_2PI * recip_standard_dev_2 * exp(-0.5 * dist2 * recip_standard_dev_2);
}

//inclusive
template <class T>
inline bool inRange(T x, T min, T max)
{
	return x >= min && x <= max;
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
inline T tanForCos(T cos_theta)
{
	assert(cos_theta >= -1.0 && cos_theta <= 1.0);

	//sin(theta)^2 + cos(theta)^2 + 1
	//sin(theta) = sqrt(1 - cos(theta)^2)
	return sqrt(1.0 - cos_theta*cos_theta) / cos_theta;
}

void test();

}


#endif //__MATHSTYPES_H__




















