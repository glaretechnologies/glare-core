/*=====================================================================
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#ifndef __TIMER_H__
#define __TIMER_H__


#include "clock.h"
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
	last_time_started = getCurTimeRealSec();
	timesofar = 0;
}

Timer::~Timer()
{}

double Timer::getSecondsElapsed() const
{
	if(paused)
		return timesofar;
	else
		return timesofar + getCurTimeRealSec() - last_time_started;


	//return GetCurTimeRealSec() - time_started;
}

void Timer::reset()
{
	last_time_started = getCurTimeRealSec();
	timesofar = 0;
}


bool Timer::isPaused()
{
	return paused;
}


void Timer::pause()
{
	assert(!paused);

	paused = true;

	timesofar += getCurTimeRealSec() - last_time_started;
}

void Timer::unpause()
{
	assert(paused);

	paused = false;

	last_time_started = getCurTimeRealSec();
}

	


#endif //__TIMER_H__
