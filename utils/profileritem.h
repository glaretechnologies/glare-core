/*=====================================================================
profileritem.h
--------------
File created by ClassTemplate on Sat Aug 23 04:39:58 2003
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PROFILERITEM_H_666_
#define __PROFILERITEM_H_666_

#pragma warning(disable : 4786)//disable long debug name warning

#include "timer.h"
#include <string>

typedef double PROFILER_REAL_TYPE;

/*=====================================================================
ProfilerItem
------------

=====================================================================*/
class ProfilerItem
{
public:
	/*=====================================================================
	ProfilerItem
	------------
	
	=====================================================================*/
	ProfilerItem(const std::string& itemname);

	~ProfilerItem();


	void start();
	void end();

	PROFILER_REAL_TYPE getTotalTime() const { return total_time; }

	const std::string& getItemName() const { return itemname; }

private:
	std::string itemname;
	Timer timer;
	PROFILER_REAL_TYPE total_time;
	bool timing;
};



#endif //__PROFILERITEM_H_666_




