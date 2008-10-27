/*=====================================================================
SSE.h
-----
File created by ClassTemplate on Sat Jun 25 08:01:25 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __SSE_H_666_
#define __SSE_H_666_

#include "../utils/platform.h"
#include <assert.h>
#ifdef COMPILER_GCC
#include <stdlib.h>
#include <inttypes.h>
#endif

namespace SSE
{
	void checkForSSE(bool& mmx_present, bool& sse1_present, bool& sse2_present, bool& sse3_present);
};



//#ifdef USE_SSE

#include <xmmintrin.h> //SSE header file

#ifdef USE_SSE2
#include <emmintrin.h> //SSE 2 header file
#endif

typedef __m128 SSE4Vec;//A vector of 4 single precision floats.  16 byte aligned by default.
#ifdef USE_SSE2
typedef __m128i SSE4Int;//A vector of 4 32 bit integers.  16 byte aligned by default.
#endif


#ifdef COMPILER_MSVC
#define SSE_ALIGN _MM_ALIGN16
#else
#define SSE_ALIGN __attribute__ ((aligned (16)))
#endif

template <class T>
inline bool isAlignedTo(T* ptr, uintptr_t alignment)
{
	return (uintptr_t(ptr) % alignment) == 0;
}

template <class T>
inline bool isSSEAligned(T* ptr)
{
	return isAlignedTo(ptr, 16);
}

#define assertSSEAligned(p) (assert(isSSEAligned((p))))

inline void* alignedMalloc(size_t size, size_t alignment)
{
#ifdef COMPILER_MSVC
	return _aligned_malloc(size, alignment);
#else
	void* mem_ptr;
	const int result = posix_memalign(&mem_ptr, alignment, size);
	assert(result == 0);
	//TODO: handle fail here somehow.
	if(result != 0)
		return (void*)0;
	return mem_ptr;
#endif
}

inline void alignedFree(void* mem)
{
#ifdef COMPILER_MSVC
	_aligned_free(mem);
#else
	// Apparently free() can handle aligned mem.
	// see: http://www.opengroup.org/onlinepubs/000095399/functions/posix_memalign.html
	free(mem);
#endif
}


inline void* alignedSSEMalloc(size_t size)
{
	return alignedMalloc(size, 16);
}

template <class T>
inline void alignedSSEFree(T* t)
{
	alignedFree((void*)t);
}

template <class T>
inline void alignedSSEArrayMalloc(size_t numelems, T*& t_out)
{
	const size_t memsize = sizeof(T) * numelems;
	t_out = static_cast<T*>(alignedSSEMalloc(memsize));
}

template <class T>
inline void alignedArrayMalloc(size_t numelems, size_t alignment, T*& t_out)
{
	const size_t memsize = sizeof(T) * numelems;
	t_out = static_cast<T*>(alignedMalloc(memsize, alignment));
}

template <class T>
inline void alignedSSEArrayFree(T* t)
{
	alignedSSEFree(t);

	//for(unsigned int i=0; i<numelems; ++i)
	//	t[i].~T();
	//alignedSSEFree(t);
	//delete(t) []
}

template <class T>
inline void alignedArrayFree(T* t)
{
	alignedFree(t);
}




const SSE_ALIGN float zero_4vec[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
const SSE_ALIGN float one_4vec[4] = { 1.0f, 1.0f, 1.0f, 1.0f };




#define load4Vec(v) (_mm_load_ps(v))
//Loads four single-precision, floating-point values. The address must be 16-byte aligned.

#define store4Vec(v, loc) (_mm_store_ps(loc, v))
//Stores four single-precision, floating-point values. The address must be 16-byte aligned.

#define storeLowWord(v, loc) (_mm_store_ss(loc, v))
//Stores the lower single-precision, floating-point value.

#define loadScalarCopy(s) (_mm_load_ps1(s))
//Loads a single single-precision, floating-point value, copying it into all four words.
//Composite op: MOVSS + shuffling

#define loadScalarLow(s) (_mm_load_ss(s))
//Loads an single-precision, floating-point value into the low word and clears the upper three words.

#define add4Vec(a, b) (_mm_add_ps((a), (b)))
//Adds the four single-precision, floating-point values of a and b.

#define sub4Vec(a, b) (_mm_sub_ps((a), (b)))
//Subtracts the four single-precision, floating-point values of a and b. (r0 := a0 - b0 etc..)

inline const SSE4Vec subLowWord(const SSE4Vec& a, const SSE4Vec& b)
{
	return _mm_sub_ss(a, b);
}

#define mult4Vec(a, b) (_mm_mul_ps((a), (b)))
//Multiplies the four single-precision, floating-point values of a and b.

#define multByScalar(a, b) ( mult4Vec( (a), loadScalarCopy((b)) ) )

#define div4Vec(a, b) (_mm_div_ps((a), (b)))
//Divides the four single-precision, floating-point values of a and b. (result = a/b)

#define recip4Vec(a) (_mm_rcp_ps(a))
//Computes the approximations of reciprocals of the four single-precision, floating-point values of a.

#define lessThan4Vec(a, b) (_mm_cmplt_ps((a), (b)))
//Compares for less than. (all components).  Sets to 0xffffffff if true, 0x0 otherwise

#define lessThanLowWord(a, b) (_mm_cmple_ss((a), (b)))
//Compares for less than. (low word only)

#define MSBMask(a) (_mm_movemask_ps(a))
//Creates a 4-bit mask from the most significant bits of the four single-precision, floating-point values.

#define allTrue(a) (MSBMask(a) == 0x0000000F)
//is true IFF the four bits are set

#define noneTrue(a) (MSBMask(a) == 0x00000000)

/*
#define and4Vec(a) (MSBMask(a) == 0x0000000F)
//is true IFF the four bits are set

#define or4Vec(a) (MSBMask(a) != 0x00000000)
//is true IFF the four bits are set*/

#define and4Vec(a, b) (_mm_and_ps((a), (b)))
//Computes the bitwise AND of the four single-precision, floating-point values of a and b.

inline const SSE4Vec andNot4Vec(const SSE4Vec& a, const SSE4Vec& b)
{
	return _mm_andnot_ps(a, b);
}

#define or4Vec(a, b) (_mm_or_ps((a), (b)))
//Computes the bitwise OR of the four single-precision, floating-point values of a and b.

//#define allLessThan(a, b) (and4Vec(lessThan4Vec(a, b)))
#define allLessThan(a, b) (allTrue(lessThan4Vec(a, b)))
//are all the values in a less than the corresponding values in b

#define min4Vec(a, b) (_mm_min_ps((a), (b)))
//Computes the minima of the four single-precision, floating-point values of a and b.

#define max4Vec(a, b) (_mm_max_ps((a), (b)))
//Computes the maximums of the four single-precision, floating-point values of a and b.


#define SHUF_X 0
#define SHUF_Y 1
#define SHUF_Z 2

//#define shuffle4Vec(a, b, x, y, z, w) (_mm_shuffle_ps(a, b, _MM_SHUFFLE(w, z, y, x)))
#define shuffle4Vec(a, b, x, y, z, w) (_mm_shuffle_ps(a, b, _MM_SHUFFLE(z, y, x, w)))
//Selects four specific single-precision, floating-point values from a and b, based on the mask i. The mask must be an immediate

#define shiftLeftOneWord(a) (_mm_slli_si128((a)), 32)

//#define setToZero(a) (_mm_setzero_ps()

inline const SSE4Vec zeroVec()
{
	return _mm_setzero_ps();
}

//sets each float in result to a if the mask is 1'd, or b if the mask is zeroed
/*inline void setMasked(const SSE4Vec& a, const SSE4Vec& b, const SSE4Vec& mask, SSE4Vec& result)
{
	result = or4Vec( and4Vec(a, mask), andNot(b, mask) );
}*/

inline const SSE4Vec setMasked(const SSE4Vec& a, const SSE4Vec& b, const SSE4Vec& mask)
{
	//const SSE4Vec masked_a = and4Vec(a, mask);
	//const SSE4Vec masked_b = andNot(b, mask);

	//return or4Vec(masked_a, masked_b);
	return or4Vec( and4Vec(mask, a), andNot4Vec(mask, b) );
}
/*inline void setIfALessThanB4Vec(const SSE4Vec& a, const SSE4Vec& b, const SSE4Vec& newvals, SSE4Vec& result)
{
	setMasked(newvals, result, lessThan4Vec(a, b), result);
}*/

inline const SSE4Vec setIfALessThanB4Vec(const SSE4Vec& a, const SSE4Vec& b, const SSE4Vec& newvals, 
										 const SSE4Vec& oldvals)
{
	return setMasked(newvals, oldvals, lessThan4Vec(a, b));
}
//------------------------------------------------------------------------
//Integer stuff
//------------------------------------------------------------------------

#ifdef USE_SSE2
//------------------------------------------------------------------------
//initialisation
//------------------------------------------------------------------------
inline const SSE4Int set4Int(int x0, int x1, int x2, int x3)
{
	return _mm_set_epi32(x3, x2, x1, x0);
}

//Sets the 4 signed 32-bit integer values to i.
inline const SSE4Int setScalarCopy(int x)
{
	return _mm_set1_epi32(x);
}

//------------------------------------------------------------------------
//storing
//------------------------------------------------------------------------

inline void store4Int(const SSE4Int& v, int* dest)
{
	_mm_store_si128((SSE4Int*)dest, v);
}

//------------------------------------------------------------------------
//conversion
//------------------------------------------------------------------------
inline const SSE4Int toSSE4Int(const SSE4Vec& v)
{
	SSE_ALIGN int temp[4];
	store4Vec(v, (float*)temp);
	return set4Int(temp[0], temp[1], temp[2], temp[3]);
}

/*inline const SSE4Int load4Int(int* x)
{
	return _mm_load_si128(x);
}*/

//------------------------------------------------------------------------
//logical operators
//------------------------------------------------------------------------
#define and4Int(a, b) (_mm_and_si128((a), (b)))
//Computes the bitwise AND of the 128-bit value in a and the 128-bit value in b.

//Computes the bitwise AND of the 128-bit value in b and the bitwise NOT of the 128-bit value in a.
inline const SSE4Int andNot(const SSE4Int& a, const SSE4Int& b)
{
	return _mm_andnot_si128(a, b);
}

#define or4Int(a, b) (_mm_or_si128((a), (b)))
//Computes the bitwise OR of the 128-bit value in a and the 128-bit value in b.


inline const SSE4Int setMasked(const SSE4Int& a, const SSE4Int& b, const SSE4Int& mask)
{
	return or4Int( and4Int(mask, a), andNot(mask, b) );
}

#endif





//NOTE: this doesn't compile in GCC
/*inline float dotSSE(const SSE4Vec& v1, const SSE4Vec& v2)
{
	const SSE4Vec prod = mult4Vec(v1, v2);
	
	return prod.m128_f32[0] + prod.m128_f32[1] + prod.m128_f32[2];
}*/

//the returned SSE4Vec will have the value of the dot product in each of the components
inline const SSE4Vec dotSSEIn4Vec(const SSE4Vec& v1, const SSE4Vec& v2)
{
	SSE4Vec prod = mult4Vec(v1, v2);
	SSE4Vec result = shuffle4Vec(prod, prod, SHUF_X, SHUF_X, SHUF_X, SHUF_X);

	//SSE4Vec shuffled = shuffle4Vec(prod, prod, SHUF_Y, 0, 0, 0);
	//shuffled.x = prod.y
	//shuffled.y = prod.z
	result = add4Vec(result, shuffle4Vec(prod, prod, SHUF_Y, SHUF_Y, SHUF_Y, SHUF_Y));
	result = add4Vec(result, shuffle4Vec(prod, prod, SHUF_Z, SHUF_Z, SHUF_Z, SHUF_Z));
	
	return result;
}



inline const SSE4Vec crossSSE(const SSE4Vec& v1, const SSE4Vec& v2)
{
	return sub4Vec( mult4Vec( shuffle4Vec(v1, v1, SHUF_Y, SHUF_Z, SHUF_X, 0), shuffle4Vec(v2, v2, SHUF_Z, SHUF_X, SHUF_Y, 0) ),
					mult4Vec( shuffle4Vec(v1, v1, SHUF_Z, SHUF_X, SHUF_Y, 0), shuffle4Vec(v2, v2, SHUF_Y, SHUF_Z, SHUF_X, 0) ) );
}

inline void reciprocalSSE(const float* vec_in, float* reciprocal_out)
{
	assertSSEAligned(vec_in);
	assertSSEAligned(reciprocal_out);

	_mm_store_ps(
		reciprocal_out,
		_mm_div_ps(
			_mm_load_ps(one_4vec),
			_mm_load_ps(vec_in)
			)
		);
}

inline void addScaledVec4SSE(const float* a, const float* b, float scale, float* vec_out)
{
	assertSSEAligned(a);
	assertSSEAligned(b);
	assertSSEAligned(vec_out);

	_mm_store_ps(
		vec_out,
		_mm_add_ps(
			_mm_load_ps(a),
			_mm_mul_ps(
				_mm_load_ps(b),
				_mm_load_ps1(&scale)
				)
			)
		);
}

/*		
#else //else if not USE_SSE

//------------------------------------------------------------------------
//
//------------------------------------------------------------------------

#include <malloc.h>

typedef float SSE4Vec[4];
typedef int SSE4Int[4];

#define SSE_ALIGN 

template <class T>
inline bool isSSEAligned(T* ptr)
{
	return true;
}

#define assertSSEAligned(p) (assert(isSSEAligned((p))))

inline void* alignedMalloc(unsigned int size, unsigned int alignment)
{
	return myAlignedMalloc(size, alignment);
}

#endif //USE_SSE

*/



/*
inline void* myAlignedMalloc(unsigned int size, unsigned int alignment)
{
	assert(alignment >= 4);
	const unsigned int usesize = size + alignment*2;
	
	char* mem = (char*)malloc(usesize);
	char* p = mem;
	//snap up to next boundary
	p += alignment - ((unsigned int)p % alignment);
	assert((unsigned int)p % alignment == 0);

	//write the original address returned by malloc at p
	*(unsigned int*)p = (unsigned int)mem;
	p += alignment;

	assert(isAlignedTo(p, alignment));
	return (void*)p;
}


inline void myAlignedFree(void* mem)
{
	//first 32 bits is location of original mem alloc
	char* original = (char*)(*(unsigned int*)mem);
	free(original);
}*/


void SSETest();

#endif //__SSE_H_666_















