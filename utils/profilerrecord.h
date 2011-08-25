/*=====================================================================
profilerrecord.h
----------------
File created by ClassTemplate on Sat Sep 13 06:25:17 2003
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PROFILERRECORD_H_666_
#define __PROFILERRECORD_H_666_

#if 0

#pragma warning(disable : 4786)//disable long debug name warning

#include "profiler.h"
#include <assert.h>




#ifdef DO_PROFILING

#define PROFILER_RECORD(s) ProfilerRecord profiler_record(s)

/*=====================================================================
ProfilerRecord
--------------

=====================================================================*/
class ProfilerRecord
{
public:
	/*=====================================================================
	ProfilerRecord
	--------------
	
	=====================================================================*/
	ProfilerRecord(const std::string& item_)
		:	item(item_)
	{
		assert(!Profiler::isNull());
		if(!Profiler::isNull())
			Profiler::getInstance().startItem(item);
	}

	~ProfilerRecord()
	{
		assert(!Profiler::isNull());
		if(!Profiler::isNull())
			Profiler::getInstance().endItem(item);
	}



private:
	std::string item;
};

#else

#define PROFILER_RECORD(s) true

class ProfilerRecord
{
public:
	ProfilerRecord(const std::string& item_)
	{}

	~ProfilerRecord()
	{}
private:
};



#endif


#endif //__PROFILERRECORD_H_666_




#endif