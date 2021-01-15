/*=====================================================================
AtomicInt.h
-----------
Copyright Glare Technologies Limited 2021
=====================================================================*/
// Not using pragma once since we copy this file and pragma once is path based
#ifndef GLARE_ATOMIC_INT_H
#define GLARE_ATOMIC_INT_H


#include "Platform.h"
#include <assert.h>
#if defined(_WIN32)
#include <intrin.h>
#endif


namespace glare
{


#if defined(__x86_64__) || defined(__ia64__) || defined(_M_X64) // If 64-bit:
typedef int64 atomic_int;
#else
typedef int32 atomic_int;
#endif


///
/// Atomic integer.
///
class AtomicInt
{
public:
	inline AtomicInt(atomic_int val_ = 0) : val(val_) { /*assert(((uint64)this % 8) == 0);*/ }
	inline ~AtomicInt() {}


	inline operator atomic_int() const { return val; }

	inline void operator = (atomic_int val_);

	inline atomic_int operator++ (int); // postfix ++ operator
	inline atomic_int operator-- (int); // postfix -- operator

	inline atomic_int operator += (atomic_int x);
	inline atomic_int operator -= (atomic_int x);

	/// Returns the old value. (value before increment)
	inline atomic_int increment();

	/// Returns the old value. (value before decrement)
	inline atomic_int decrement();

	static void test();

private:
	//GLARE_DISABLE_COPY(IndigoAtomic);

	// The volatile keyword here is required, otherwise, for example, Visual C++ will hoist the load out of a while loop.
	volatile atomic_int val;
};


inline void AtomicInt::operator = (atomic_int val_)
{
	val = val_;
}


inline atomic_int AtomicInt::operator++ (int)
{
	return increment();
}


inline atomic_int AtomicInt::operator-- (int)
{
	return decrement();
}


//----------------- Define an atomicAdd function ---------------
#if defined(_WIN32)

#if defined(__x86_64__) || defined(__ia64__) || defined(_M_X64) // If 64-bit:

GLARE_STRONG_INLINE atomic_int atomicAdd(volatile atomic_int* val, const atomic_int delta)
{
	return _InterlockedExchangeAdd64(val, delta);
}

#else // Else 32 bit build:

GLARE_STRONG_INLINE glare_atomic_int atomicAdd(volatile atomic_int* val, const atomic_int delta)
{
	return _InterlockedExchangeAdd((volatile long*)val, delta);
}

#endif

#else // else if !defined(_WIN32):

#if defined(__x86_64__) || defined(__ia64__) || defined(_M_X64) // If 64-bit:

GLARE_STRONG_INLINE int64 atomicAdd(int64 volatile* value, int64 input)
{
	return __sync_fetch_and_add(value, input);
}

#else // Else 32 bit build:

GLARE_STRONG_INLINE int32 atomicAdd(int32 volatile* value, int32 input)
{  
	return __sync_fetch_and_add(value, input);
}

#endif

#endif // !defined(_WIN32)
//----------------- End Define an atomicAdd function ---------------


inline atomic_int AtomicInt::increment()
{
	return atomicAdd(&val, 1);
}


inline atomic_int AtomicInt::decrement()
{
	return atomicAdd(&val, -1);
}


inline atomic_int AtomicInt::operator += (atomic_int x)
{
	return atomicAdd(&val, x);
}


inline atomic_int AtomicInt::operator -= (atomic_int x)
{
	return atomicAdd(&val, -x);
}


} // end namespace glare


#endif // GLARE_ATOMIC_INT_H
