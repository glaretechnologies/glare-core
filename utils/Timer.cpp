/*=====================================================================
Timer.cpp
---------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "Timer.h"


#include "StringUtils.h"


const std::string Timer::elapsedString() const // e.g "30.4 s"
{
	return toString(this->elapsed()) + " s";
}


const std::string Timer::elapsedStringNPlaces(int n) const // Print with n decimal places.
{
	return ::doubleToStringNDecimalPlaces(this->elapsed(), n) + " s";
}


const std::string Timer::elapsedStringNSigFigs(int n) const // Print with n decimal places.
{
	return ::doubleToStringNSigFigs(this->elapsed(), n) + " s";
}


const std::string Timer::elapsedStringMSWIthNSigFigs(int n) const
{
	return ::doubleToStringNSigFigs(this->elapsed() * 1000, n) + " ms";
}


const std::string Timer::elapsedStringNSWIthNSigFigs(int n) const
{
	return ::doubleToStringNSigFigs(this->elapsed() * 1.0e9, n) + " ns";
}


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "PlatformUtils.h"


void Timer::test()
{
	{
		Timer t;
		t.pause();
		const double elapsed_a = t.elapsed();
		PlatformUtils::Sleep(0);
		const double elapsed_b = t.elapsed();
		testAssert(elapsed_a == elapsed_b);

		// Test calling pause twice doesn't increase the time.
		t.pause();
		testAssert(t.elapsed() == elapsed_a);

		t.unpause();
		PlatformUtils::Sleep(1);
		testAssert(t.elapsed() > elapsed_a);
	}
}


#endif // BUILD_TESTS
