/*=====================================================================
TimerQueue.h
------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <vector>
#include <functional>


class TimerQueueTimer
{
public:
	uint64 id;
	double trigger_time;
	double period;
	bool repeating;
	std::function<void()> timer_event_func;
};


/*=====================================================================
TimerQueue
----------
Allows adding a timer that when reaches its trigger time will call a function.
Uses a priority queue for efficient insertion of events.

Use a std::vector with std::push_heap etc. instead of std::priority_queue so that
we can iterate over the vector, that std::priority_queue doesn't allow.
=====================================================================*/
class TimerQueue
{
public:
	TimerQueue();
	~TimerQueue();

	typedef uint64 TimerID;

	TimerID addTimer(double period, bool repeating, std::function<void()> timer_event_func);

	void cancelTimer(TimerID id);

	void think();

	static void test();
private:
	struct TimerComparator
	{
		bool operator() (const TimerQueueTimer& a, const TimerQueueTimer& b) const { return a.trigger_time > b.trigger_time; }
	};

	std::vector<TimerQueueTimer> queue;

	TimerID next_timer_id;
};
