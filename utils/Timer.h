/*=====================================================================
Timer.h
-------------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "Clock.h"
#include <assert.h>
#include <string>


class Timer
{
public:
	inline Timer();

	inline void setSecondsElapsed(double elapsed_seconds) { time_so_far = elapsed_seconds; }
	inline double getSecondsElapsed() const;
	inline double elapsed() const { return getSecondsElapsed(); }
	const std::string elapsedString() const; // e.g "30.4 s"
	const std::string elapsedStringNPlaces(int n) const; // Print with n decimal places.
	const std::string elapsedStringNSigFigs(int n) const; // Print with n significan figures.

	inline void reset();

	inline bool isPaused();

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


bool Timer::isPaused()
{
	return paused;
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
