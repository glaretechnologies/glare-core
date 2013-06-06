/*=====================================================================
clock.h
-------------------
Copyright Glare Technologies Limited 2013 -
=====================================================================*/
#pragma once


#include <string>
#include <time.h>


// This is time in seconds since commencement of program.
// IMPORTANT NOTE: must call Clock::init() first.
double getCurTimeRealSec();

const std::string getAsciiTime(); // Get current time as nicely formatted string
const std::string getAsciiTime(time_t t); // Get current time as nicely formatted string

time_t getSecsSince1970();

// Returns a string like '2 h, 24 m, 12 s'
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


void test();

}
