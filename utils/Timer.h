/*=====================================================================
Timer.h
-------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Clock.h"
#include "ConPrint.h"
#include <assert.h>
#include <string>


class Timer
{
public:
	inline Timer(); // Timer is started upon construction.

	inline void setSecondsElapsed(double elapsed_seconds) { time_so_far = elapsed_seconds; }
	inline double getSecondsElapsed() const;
	inline double elapsed() const { return getSecondsElapsed(); }
	const std::string elapsedString() const; // e.g "30.4 s"
	const std::string elapsedStringNPlaces(int n) const; // Print with n decimal places.
	const std::string elapsedStringNSigFigs(int n) const; // Print with n significant figures.
	const std::string elapsedStringMS() const { return elapsedStringMSWIthNSigFigs(); } // Print number of milliseconds elapsed, e.g. "1.342 ms"
	const std::string elapsedStringMSWIthNSigFigs(int n = 4) const; // Print number of milliseconds elapsed, e.g. "1.34 ms"
	const std::string elapsedStringNSWIthNSigFigs(int n) const; // Print number of nanoseconds elapsed, e.g. "1.34 ns"

	inline void reset();
	inline void resetAndUnpause();

	inline bool isPaused() const { return paused; }
	inline bool isRunning() const { return !paused; }

	inline void pause(); // Has no effect if already paused.
	inline void unpause(); // Has no effect if already running.

	static void test();
private:
	double last_time_started;
	double time_so_far; // Total time of previous periods before being paused.
	bool paused;
};


Timer::Timer()
{
	last_time_started = Clock::getCurTimeRealSec();
	time_so_far = 0;
	paused = false;
}


double Timer::getSecondsElapsed() const
{
	if(paused)
		return time_so_far;
	else
		return time_so_far + Clock::getCurTimeRealSec() - last_time_started;
}


void Timer::reset()
{
	last_time_started = Clock::getCurTimeRealSec();
	time_so_far = 0;
}


void Timer::resetAndUnpause()
{
	last_time_started = Clock::getCurTimeRealSec();
	time_so_far = 0;
	paused = false;
}


void Timer::pause()
{
	if(!paused)
	{
		paused = true;
		time_so_far += Clock::getCurTimeRealSec() - last_time_started;
	}
}


void Timer::unpause()
{
	if(paused)
	{
		paused = false;
		last_time_started = Clock::getCurTimeRealSec();
	}
}


// A little utility class for printing out a message when a timer goes out of scope.
class ScopeTimer
{
public:
	ScopeTimer(const char* name_) : name(name_) {}
	~ScopeTimer()
	{
		conPrint(std::string(name) + " took " + timer.elapsedStringNSigFigs(4));
	}

	Timer timer;
	const char* name;
};
