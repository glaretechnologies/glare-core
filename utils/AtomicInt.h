/*=====================================================================
AtomicInt.h
-----------
Copyright Glare Technologies Limited 2022
=====================================================================*/
// Not using pragma once since we copy this file and pragma once is path based
#ifndef GLARE_ATOMIC_INT_H
#define GLARE_ATOMIC_INT_H


#include "Platform.h"
#include <assert.h>
#include <atomic>


namespace glare
{


#if defined(__x86_64__) || defined(__ia64__) || defined(_M_X64) // If 64-bit:
typedef int64 atomic_int;
#else
typedef int32 atomic_int;
#endif


///
/// Atomic integer.
/// Note: trying std::atomic to see if it plays nicely with Thread Sanitizer.
///
class AtomicInt
{
public:
	explicit inline AtomicInt(atomic_int val_ = 0) : val(val_) { /*assert(((uint64)this % 8) == 0);*/ }
	inline AtomicInt(const AtomicInt& other) : val(other.val) {}
	inline ~AtomicInt() {}

	inline operator atomic_int() const;

	inline int64 getVal() const;

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
	//volatile atomic_int val;
	volatile std::atomic<int64> val;
};


inline AtomicInt::operator atomic_int() const
{ 
	return val.load(std::memory_order_acquire);
}


inline int64 AtomicInt::getVal() const
{ 
	return val.load(std::memory_order_acquire);
}


inline void AtomicInt::operator = (atomic_int newval)
{
	val.store(newval, std::memory_order_release);
//#if defined(_WIN32)
//	_InterlockedExchange64(&val, newval);
//#else
//	__sync_lock_test_and_set(&val, newval);
//#endif
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
// #if defined(_WIN32)
// 
// #if defined(__x86_64__) || defined(__ia64__) || defined(_M_X64) // If 64-bit:
// 
// GLARE_STRONG_INLINE atomic_int atomicAdd(volatile atomic_int* val, const atomic_int delta)
// {
// 	return _InterlockedExchangeAdd64(val, delta);
// }
// 
// #else // Else 32 bit build:
// 
// GLARE_STRONG_INLINE glare_atomic_int atomicAdd(volatile atomic_int* val, const atomic_int delta)
// {
// 	return _InterlockedExchangeAdd((volatile long*)val, delta);
// }
// 
// #endif
// 
// #else // else if !defined(_WIN32):
// 
// #if defined(__x86_64__) || defined(__ia64__) || defined(_M_X64) // If 64-bit:
// 
// GLARE_STRONG_INLINE int64 atomicAdd(int64 volatile* value, int64 input)
// {
// 	return __sync_fetch_and_add(value, input);
// }
// 
// #else // Else 32 bit build:
// 
// GLARE_STRONG_INLINE int32 atomicAdd(int32 volatile* value, int32 input)
// {  
// 	return __sync_fetch_and_add(value, input);
// }
// 
// #endif
// 
// #endif // !defined(_WIN32)
//----------------- End Define an atomicAdd function ---------------


// Returns old value
inline atomic_int AtomicInt::increment()
{
	return val.fetch_add(1, std::memory_order_acq_rel);
	//return atomicAdd(&val, 1);
}


// Returns old value
inline atomic_int AtomicInt::decrement()
{
	return val.fetch_add(-1, std::memory_order_acq_rel);
	//return atomicAdd(&val, -1);
}


inline atomic_int AtomicInt::operator += (atomic_int x)
{
	return val.fetch_add(x, std::memory_order_acq_rel);
	//return atomicAdd(&val, x);
}


inline atomic_int AtomicInt::operator -= (atomic_int x)
{
	return val.fetch_sub(x, std::memory_order_acq_rel);
	//return atomicAdd(&val, -x);
}


} // end namespace glare


#endif // GLARE_ATOMIC_INT_H
