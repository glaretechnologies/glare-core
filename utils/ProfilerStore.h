/*=====================================================================
ProfilerStore.h
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2016-11-01 23:24:09 +0000
=====================================================================*/
#pragma once


#include "Mutex.h"
#include "Clock.h"
#include "../graphics/colour3.h"
#include <vector>
#include <string>
class ProfilerStore;


//#define ENABLE_PROFILING 1


struct ProfileEvent
{
	ProfileEvent() : col(0.5f), thread_index(0) {}

	std::string name;
	Colour3f col;
	double event_time;
	int thread_index;
};


struct ProfileInterval
{
	ProfileInterval() : col(0.5f), thread_index(0) {}

	std::string name;
	Colour3f col;
	double start_time;
	double end_time;
	int thread_index;
};


/*=====================================================================
ProfilerStore
-------------

=====================================================================*/
class ProfilerStore
{
public:
	ProfilerStore();
	~ProfilerStore();

	static ProfilerStore* getInstance();

#if ENABLE_PROFILING
	static void recordProfileEvent(const char* scope_name_, int thread_index_ = 0); // thread-safe
#else
	static inline void recordProfileEvent(const char* scope_name_, int thread_index_ = 0) {}
#endif



	void addEvent(const ProfileEvent& ev);
	void addInterval(const ProfileInterval& interval);


	std::vector<ProfileEvent> events;
	std::vector<ProfileInterval> intervals;
	double creation_time;
	Mutex mutex;
private:
	
};


// ScopeProfiler - Uses RAII to record an interval using a scope.  Just create an instance at the start of a scope.
#if ENABLE_PROFILING

struct ScopeProfiler
{
	ScopeProfiler(const char* scope_name_, int thread_index_ = 0);
	~ScopeProfiler();

private:
	int thread_index;
	std::string scope_name;
	double start_time;
};

#else

struct ScopeProfiler
{
	ScopeProfiler(const char* scope_name_, int thread_index_ = 0) {}
};

#endif
