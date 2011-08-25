/*=====================================================================
profiler.h
----------
File created by ClassTemplate on Sat Aug 23 04:35:35 2003
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PROFILER_H_666_
#define __PROFILER_H_666_

#error NOT USED ANY MORE

#include "timer.h"
class ProfilerItem;
#include <ostream>
#include <map>
#include "../utils/singleton.h"

//#define DO_PROFILING true

/*=====================================================================
Profiler
--------

=====================================================================*/
class Profiler : public Singleton<Profiler>
{
public:
	/*=====================================================================
	Profiler
	--------
	
	=====================================================================*/
	Profiler();

	~Profiler();

	void reset();

	void startItem(const std::string& item);
	void endItem(const std::string& item);

	void printResults(std::ostream& stream) const;

private:
	typedef std::map<std::string, ProfilerItem*> ITEM_MAP;
	ITEM_MAP items;

	//typedef std::map<std::string, Timer> TIMER_MAP;
	//TIMER_MAP timers;

	Timer profiler_lifetimer;
};




#ifdef DO_PROFILING
#define START_PROFILING(s) startProfiling(s)
#else
#define START_PROFILING(s) true
#endif

#ifdef DO_PROFILING
#define STOP_PROFILING(s) stopProfiling(s)
#else
#define STOP_PROFILING(s) true
#endif

inline void startProfiling(const std::string& item)
{
	Profiler::getInstance().startItem(item);
}

inline void stopProfiling(const std::string& item)
{
	Profiler::getInstance().endItem(item);
}




#endif //__PROFILER_H_666_




