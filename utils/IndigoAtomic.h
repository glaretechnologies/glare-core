/*=====================================================================
IndigoAtomic.h
--------------
Copyright Glare Technologies Limited 2014 -
Generated at Wed Nov 17 12:56:10 +1300 2010
=====================================================================*/
#pragma once


#include "Platform.h"
#include <assert.h>
#if defined(_WIN32)
#include <intrin.h>
#endif


#if defined(__x86_64__) || defined(__ia64__) || defined(_M_X64) // If 64-bit:
typedef int64 glare_atomic_int;
#else
typedef int32 glare_atomic_int;
#endif


/*=====================================================================
IndigoAtomic
------------
Code based on Embree's atomic.h
=====================================================================*/
class IndigoAtomic
{
public:
	inline IndigoAtomic(glare_atomic_int val_ = 0) : val(val_) { /*assert(((uint64)this % 8) == 0);*/ }
	inline ~IndigoAtomic() {}


	inline operator glare_atomic_int() const { return val; }

	inline void operator = (glare_atomic_int val_);

	// Returns the old value. (value before increment)
	inline glare_atomic_int increment();

	// Returns the old value. (value before decrement)
	inline glare_atomic_int decrement();

private:
	INDIGO_DISABLE_COPY(IndigoAtomic);

	glare_atomic_int val;
};


inline void IndigoAtomic::operator = (glare_atomic_int val_)
{
	val = val_;
}


//----------------- Define an atomicAdd function ---------------
#if defined(_WIN32)

#if defined(__x86_64__) || defined(__ia64__) || defined(_M_X64) // If 64-bit:

INDIGO_STRONG_INLINE glare_atomic_int atomicAdd(volatile glare_atomic_int* val, const glare_atomic_int delta)
{
	return _InterlockedExchangeAdd64(val, delta);
}

#else // Else 32 bit build:

INDIGO_STRONG_INLINE glare_atomic_int atomicAdd(volatile glare_atomic_int* val, const glare_atomic_int delta)
{
	return _InterlockedExchangeAdd(val, 1);
}

#endif

#else // else if !defined(_WIN32):

#if defined(__x86_64__) || defined(__ia64__) || defined(_M_X64) // If 64-bit:

INDIGO_STRONG_INLINE int64 atomicAdd(int64 volatile* value, int64 input)
{
	return __sync_fetch_and_add(value, input);

	//asm volatile("lock xaddq %0,%1" : "+r" (input), "+m" (*value) : "r" (input), "m" (*value));
	//return input;
}

#else // Else 32 bit build:

INDIGO_STRONG_INLINE int32 atomicAdd(int32 volatile* value, int32 input)
{  
	return __sync_fetch_and_add(value, input);

	//asm volatile("lock xadd %0,%1" : "+r" (input), "+m" (*value) : "r" (input), "m" (*value));
	//return input;
}

#endif

#endif // !defined(_WIN32)
//----------------- End Define an atomicAdd function ---------------


inline glare_atomic_int IndigoAtomic::increment()
{
	return atomicAdd(&val, 1);
}

inline glare_atomic_int IndigoAtomic::decrement()
{
	return atomicAdd(&val, -1);
}
