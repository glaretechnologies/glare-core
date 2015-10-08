/*=====================================================================
Timer.h
-------------------
Copyright Glare Technologies Limited 2014 -
=====================================================================*/
#pragma once


#include "Clock.h"
#include <assert.h>
#include <string>


class Timer
{
public:
	inline Timer();
	inline ~Timer();

	inline double getSecondsElapsed() const;
	inline double elapsed() const { return getSecondsElapsed(); }
	const std::string elapsedString() const; // e.g "30.4 s"
	const std::string elapsedStringNPlaces(int n) const; // Print with n decimal places.
	const std::string elapsedStringNSigFigs(int n) const; // Print with n significan figures.

	inline void reset();

	inline bool isPaused();

	inline void pause();
	inline void unpause();

private:
	double last_time_started;
	double timesofar;
	bool paused;
};


Timer::Timer()
{
	paused = false;
	last_time_started = Clock::getCurTimeRealSec();
	timesofar = 0;
}

Timer::~Timer()
{}

double Timer::getSecondsElapsed() const
{
	if(paused)
		return timesofar;
	else
		return timesofar + Clock::getCurTimeRealSec() - last_time_started;


	//return GetCurTimeRealSec() - time_started;
}

void Timer::reset()
{
	last_time_started = Clock::getCurTimeRealSec();
	timesofar = 0;
}


bool Timer::isPaused()
{
	return paused;
}


void Timer::pause()
{
//	assert(!paused);

	paused = true;

	timesofar += Clock::getCurTimeRealSec() - last_time_started;
}

void Timer::unpause()
{
//	assert(paused);

	paused = false;

	last_time_started = Clock::getCurTimeRealSec();
}
