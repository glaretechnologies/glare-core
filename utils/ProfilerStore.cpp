/*=====================================================================
ProfilerStore.cpp
-------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "ProfilerStore.h"


#include "Lock.h"
#include "IncludeXXHash.h"


ProfilerStore::ProfilerStore()
{
	creation_time = Clock::getCurTimeRealSec();
}


ProfilerStore::~ProfilerStore()
{

}


static Mutex instance_mutex;
static ProfilerStore* instance = 0;


ProfilerStore* ProfilerStore::getInstance()
{
	Lock lock(instance_mutex);
	if(instance == 0)
		instance = new ProfilerStore();
	return instance;
}


void ProfilerStore::addEvent(const ProfileEvent& ev)
{
	Lock lock(mutex);
	if(events.size() < 10000) // Put a limit on the number of events for now
		events.push_back(ev);
}


void ProfilerStore::addInterval(const ProfileInterval& interval)
{
	Lock lock(mutex);
	if(intervals.size() < 10000) // Put a limit on the number of intervals for now
		intervals.push_back(interval);
}


#if ENABLE_PROFILING

void ProfilerStore::recordProfileEvent(const char* scope_name_, int thread_index_)
{
	ProfileEvent ev;
	ev.event_time = Clock::getCurTimeRealSec() - ProfilerStore::getInstance()->creation_time;
	ev.name = scope_name_;
	// Pick a quasi-random colour
	ev.col.r = XXH64(ev.name.data(), ev.name.size(), 1) / (float)std::numeric_limits<uint64>::max();
	ev.col.g = XXH64(ev.name.data(), ev.name.size(), 2) / (float)std::numeric_limits<uint64>::max();
	ev.col.b = XXH64(ev.name.data(), ev.name.size(), 3) / (float)std::numeric_limits<uint64>::max();
	ev.thread_index = thread_index_;
	
	
	ProfilerStore::getInstance()->addEvent(ev);
}


ScopeProfiler::ScopeProfiler(const char* scope_name_, int thread_index_)
:	scope_name(scope_name_), thread_index(thread_index_)
{
	start_time = Clock::getCurTimeRealSec() - ProfilerStore::getInstance()->creation_time;
}


ScopeProfiler::~ScopeProfiler()
{
	ProfileInterval interval;
	interval.end_time = Clock::getCurTimeRealSec() - ProfilerStore::getInstance()->creation_time;
	interval.thread_index = thread_index;
	// Pick a quasi-random colour
	interval.col.r = XXH64(scope_name.data(), scope_name.size(), 1) / (float)std::numeric_limits<uint64>::max();
	interval.col.g = XXH64(scope_name.data(), scope_name.size(), 2) / (float)std::numeric_limits<uint64>::max();
	interval.col.b = XXH64(scope_name.data(), scope_name.size(), 3) / (float)std::numeric_limits<uint64>::max();
	interval.name = scope_name;
	interval.start_time = start_time;
	ProfilerStore::getInstance()->addInterval(interval);
}

#endif // ENABLE_PROFILING
