/*=====================================================================
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#include "clock.h"
	

#if defined(WIN32) || defined(WIN64)
// Stop windows.h from defining the min() and max() macros
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <assert.h>
#include "stringutils.h"


static bool clock_initialised = false;
static double clock_period = 0;


void Clock::init()
{
#if defined(WIN32) || defined(WIN64)
	LARGE_INTEGER freq;
	const BOOL b = QueryPerformanceFrequency(&freq);
	assert(b);

	clock_period = 1.0 / (double)(freq.QuadPart);
#else
	clock_period = 0.0;
#endif

	clock_initialised = true;
}


double getCurTimeRealSec()
{
	assert(clock_initialised);

#if defined(WIN32) || defined(WIN64)
	LARGE_INTEGER count;
	const BOOL b = QueryPerformanceCounter(&count); 
	assert(b);

	return (double)(count.QuadPart) * clock_period;
#else

	/* struct timeval {
               time_t         tv_sec;    // seconds
               suseconds_t    tv_usec;  // microseconds
       };
*/
	struct timeval t;
	gettimeofday(&t, NULL);
	return ((double)t.tv_sec + (double)t.tv_usec * 0.000001);
#endif
}


const std::string getAsciiTime()
{
	time_t currenttime;

	time(&currenttime);                 
	
	struct tm *newtime = localtime(&currenttime);
	if(!newtime)
		return "";
                                   
	std::string s = asctime(newtime);

	if(!s.empty() && s[(int)s.size() - 1] == '\n')
		s = s.substr(0, (int)s.size() - 1);

	return s;
}


const std::string getAsciiTime(time_t t)//nicely formatted string
{
	struct tm *newtime = localtime(&t);
	if(!newtime)
		return "";
                                   
	std::string s = asctime(newtime);

	if(!s.empty() && s[(int)s.size() - 1] == '\n')
		s = s.substr(0, (int)s.size() - 1);

	return s;
}


time_t getSecsSince1970()
{
	return time(NULL);                 
}


/*bool leftTimeEarlier(const std::string& time_a, const std::string& time_b)
{
	return true;
}*/


const std::string humanReadableDuration(int seconds)
{
	if(seconds < 0)
	    seconds = 0;

	const int hours = seconds / 3600;
	seconds -= hours * 3600;
	const int minutes = seconds / 60;
	seconds -= minutes * 60;
	
	std::string s;
	if(hours > 0)
	    s += ::toString(hours) + " h, ";
	if(minutes > 0)
	    s += ::toString(minutes) + " m, ";
	s += ::toString(seconds) + " s";

	return s;
}


namespace Clock
{

/*
	day = Day of month (1 – 31).
	month = Month (0 – 11; January = 0).
	year = e.g. 2009
*/
void getCurrentDay(int& day, int& month, int& year)
{
	time_t t = time(NULL);
	const tm* thetime = localtime(&t);

	day = thetime->tm_mday; // Day of month (1 – 31).
	month = thetime->tm_mon; // Month (0 – 11; January = 0).
	year = thetime->tm_year + 1900; // tm_year = Year (current year minus 1900).
}


}
