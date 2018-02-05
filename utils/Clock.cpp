/*=====================================================================
Clock.cpp
---------
Copyright Glare Technologies Limited 2017 -
=====================================================================*/
#include "Clock.h"


#include "IncludeWindows.h"
#include "StringUtils.h"
#include "Platform.h"
#include <assert.h>
#if !defined(_WIN32)
#include <sys/time.h>
#endif


namespace Clock
{


static bool clock_initialised = false;
#if defined(_WIN32)
static double clock_period = 0;
#endif
static double init_time = 0;


void init()
{
#if defined(_WIN32)
	LARGE_INTEGER freq;
	const BOOL b = QueryPerformanceFrequency(&freq);
	assertOrDeclareUsed(b);

	clock_period = 1.0 / (double)freq.QuadPart;
#endif

	clock_initialised = true;

	init_time = getCurTimeRealSec();
}


double getCurTimeRealSec()
{
	assert(clock_initialised);

#if defined(_WIN32)
	LARGE_INTEGER count;
	const BOOL b = QueryPerformanceCounter(&count); 
	assertOrDeclareUsed(b);

	return (double)count.QuadPart * clock_period;
#else

	/* struct timeval {
               time_t         tv_sec;    // seconds
               suseconds_t    tv_usec;  // microseconds
       };
	*/
	struct timeval t;
	gettimeofday(&t, NULL);
	return (double)t.tv_sec + (double)t.tv_usec * 0.000001;
#endif
}


double getTimeSinceInit()
{
	return getCurTimeRealSec() - init_time;
}


const std::string getAsciiTime()
{
	// Get current system time
	time_t current_time;
	time(&current_time);    

	return getAsciiTime(current_time);
}


const std::string getAsciiTime(time_t t)
{
#ifdef _WIN32
	// NOTE: We will use localtime_s and asctime_s, as they seem to be safer than localtime and asctime.

	struct tm local_time;
	if(localtime_s(&local_time, &t) != 0)
	{
		// Error occurred.
		assert(0);
		return "";
	}
	
	char buf[32];
	if(asctime_s(buf, sizeof(buf), &local_time) != 0)
	{
		// Error occurred
		assert(0);
		return "";
	}

	// Remove trailing newline character
	return ::stripTailWhitespace(std::string(buf));
#else
	// NOTE: We will use the thread-safe versions (with an _r suffix) of localtime and asctime.

	struct tm local_time;
	if(localtime_r(&t, &local_time) == 0)
	{
		// Error occurred.
		assert(0);
		return "";
	}
	
	// The asctime_r() function does the same, but stores the string in a user-supplied buffer which should have room for at least 26 bytes.
	char buf[32];
	if(asctime_r(&local_time, buf) == 0)
	{
		// Error occurred
		assert(0);
		return "";
	}

	// Remove trailing newline character
	return ::stripTailWhitespace(std::string(buf));
#endif
}


time_t getSecsSince1970()
{
	return time(NULL);                 
}


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


//-------------------------------------------------------------------------------------------


// day_of_week = days since Sunday - [0,6]
const std::string dayAsString(int day_of_week)
{
	switch(day_of_week)
	{
	case 0:
		return "Sun";
	case 1:
		return "Mon";
	case 2:
		return "Tue";
	case 3:
		return "Wed";
	case 4:
		return "Thu";
	case 5:
		return "Fri";
	case 6:
		return "Sat";
	default:
		assert(0);
		return "";
	};
}


const std::string monthString(int month)
{
	std::string monthstr;
	switch(month)
	{
	case 0:
		monthstr = "Jan";
		break;
	case 1:
		monthstr = "Feb";
		break;
	case 2:
		monthstr = "Mar";
		break;
	case 3:
		monthstr = "Apr";
		break;
	case 4:
		monthstr = "May";
		break;
	case 5:
		monthstr = "Jun";
		break;
	case 6:
		monthstr = "Jul";
		break;
	case 7:
		monthstr = "Aug";
		break;
	case 8:
		monthstr = "Sep";
		break;
	case 9:
		monthstr = "Oct";
		break;
	case 10:
		monthstr = "Nov";
		break;
	case 11:
		monthstr = "Dec";
		break;
	default:
		assert(0);
		break;
	};
	return monthstr;
}


const std::string twoDigitString(int x)
{
	return ::leftPad(::toString(x), '0', 2);
}


const std::string RFC822FormatedString() // http://www.faqs.org/rfcs/rfc822.html
{
	return RFC822FormatedString(time(NULL));
}

const std::string RFC822FormatedString(time_t t) // http://www.faqs.org/rfcs/rfc822.html
{
	tm thetime;
	// Get calender time in UTC.  Use threadsafe versions of gmtime.
#ifdef _WIN32
	gmtime_s(&thetime, &t);
#else
	gmtime_r(&t, &thetime);
#endif

	const int day_of_week = thetime.tm_wday; // days since Sunday - [0,6]
	const int day = thetime.tm_mday; // Day of month (1 – 31).
	const int month = thetime.tm_mon; // Month (0 – 11; January = 0).
	const int year = thetime.tm_year + 1900; // tm_year = Year (current year minus 1900).

	return dayAsString(day_of_week) + ", " + toString(day) + " " + monthString(month) + " " + toString(year) + " " +
		twoDigitString(thetime.tm_hour) + ":" + twoDigitString(thetime.tm_min) + ":" + twoDigitString(thetime.tm_sec) + " GMT";
}


} // end namespace Clock


#if BUILD_TESTS


#include "ConPrint.h"
#include "../indigo/TestUtils.h"


void Clock::test()
{
	conPrint("Clock::test()");

	getSecsSince1970();

	{
		const std::string time_str = getAsciiTime();

		testAssert(!time_str.empty());
		conPrint("getAsciiTime(): '" + time_str + "'");
	}
}


#endif // BUILD_TESTS
