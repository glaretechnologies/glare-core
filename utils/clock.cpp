/*=====================================================================
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#include "clock.h"
	
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <assert.h>
//#include <time.h>
#include "stringutils.h"

//returns current time in seconds
double getCurTimeRealSec()
{
#ifdef WIN32
	/*static */LARGE_INTEGER li_net;
	memset(&li_net, 0, sizeof(li_net));
	BOOL b = QueryPerformanceCounter(&li_net); 
	assert(b);


	/*static */LARGE_INTEGER freq;
	memset(&freq, 0, sizeof(freq));
	b = QueryPerformanceFrequency(&freq);
	assert(b);


	return (double)(li_net.QuadPart) / (double)(freq.QuadPart);
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
	//NOTE: check for mem leak here
                                   
	std::string s = asctime(newtime);

	if(!s.empty() && s[(int)s.size() - 1] == '\n')
		s = s.substr(0, (int)s.size() - 1);

	return s;
}


time_t getSecsSince1970()
{
	return time(NULL);                 
}

bool leftTimeEarlier(const std::string& time_a, const std::string& time_b)
{


	return true;
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

















