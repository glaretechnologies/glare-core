/*=====================================================================
Clock.h
-------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include <string>
#include <time.h>


namespace Clock
{


// This needs to be called before getCurTimeRealSec() is used.
void init();


// IMPORTANT NOTE: must call Clock::init() first.
double getCurTimeRealSec();


double getTimeSinceInit(); // In seconds, since init() was called.


time_t getSecsSince1970(); // Seconds since midnight, January 1, 1970 UTC.


// Returns a string like '2 h, 24 m, 12 s'
const std::string humanReadableDuration(int seconds);


const std::string getAsciiTime(); // Get time as nicely formatted string
const std::string getAsciiTime(time_t t); // Get current time as nicely formatted string


// See http://www.faqs.org/rfcs/rfc822.html
const std::string RFC822FormatedString(); // Returns the current time, formatted.
const std::string RFC822FormatedString(time_t t); // Get time, formatted


std::string get12HourClockLocalTimeOfDayString(); // E.g. "2:34:50 pm"

/*
	day = Day of month (1 – 31).
	month = Month (0 – 11; January = 0).
	year = e.g. 2009
*/
void getCurrentDay(int& day, int& month, int& year);


// Returns days since January 1 - [0, 365], and the year
// also hours since midnight - [0, 23]
void getHourDayOfYearAndYear(time_t t, int& hour_out, int& day_out, int& year_out);


void test();

}
