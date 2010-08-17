/*=====================================================================
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#ifndef __CLOCK_H_666_
#define __CLOCK_H_666_

#include <string>
#include <time.h>


// This is time in seconds since commencement of program.
// IMPORTANT NOTE: must call Clock::init() first.
double getCurTimeRealSec();


const std::string getAsciiTime();//nicely formatted string
const std::string getAsciiTime(time_t t);//nicely formatted string

//bool leftTimeEarlier(const std::string& asciitime_a, const std::string& asciitime_b);

time_t getSecsSince1970();//hehe

const std::string humanReadableDuration(int seconds);

namespace Clock
{
	void init();
/*
	day = Day of month (1 – 31).
	month = Month (0 – 11; January = 0).
	year = e.g. 2009
*/
void getCurrentDay(int& day, int& month, int& year);
}

#endif //__CLOCK_H_666_
