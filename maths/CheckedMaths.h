/*=====================================================================
CheckedMaths.h
--------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "mathstypes.h"
#include "../utils/RuntimeCheck.h"


namespace CheckedMaths
{


// Triggers assert and throws glare::Exception if the result would overflow.
template <class T> 
T addUnsignedInts(T x, T y)
{
	static_assert(std::numeric_limits<T>::is_integer, "Template param must be an integer");
	static_assert(!std::numeric_limits<T>::is_signed, "Template param must not be signed");

	runtimeCheck(!Maths::unsignedIntAdditionWraps(x, y));
	return x + y;
}


void test();


} // end namespace CheckedMaths
