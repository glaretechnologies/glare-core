/*=====================================================================
TimerQueue.cpp
--------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "TimerQueue.h"


#include "Clock.h"
#include <cassert>
#include <algorithm>
#include <limits>


TimerQueue::TimerQueue()
:	next_timer_id(0)
{}


TimerQueue::~TimerQueue()
{}


TimerQueue::TimerID TimerQueue::addTimer(double period, bool repeating, std::function<void()> timer_event_func)
{
	if(repeating)
	{
		assert(period > 0);
		if(period <= 0)
			return std::numeric_limits<TimerID>::max(); // Avoid infinite loop
	}

	const double cur_time = Clock::getCurTimeRealSec();

	TimerQueueTimer timer;
	timer.id = next_timer_id++;
	timer.trigger_time = cur_time + period;
	timer.period = period;
	timer.repeating = repeating;
	timer.timer_event_func = timer_event_func;

	// Push timer into priority queue
	queue.push_back(timer);
	std::push_heap(queue.begin(), queue.end(), TimerComparator());

	return timer.id;
}


void TimerQueue::cancelTimer(TimerID id)
{
	for(size_t i=0; i<queue.size(); ++i)
		if(queue[i].id == id)
		{
			queue[i].timer_event_func = std::function<void()>(); // clear function
			queue[i].repeating = false;
			return;
		}
}


void TimerQueue::think()
{
	const double cur_time = Clock::getCurTimeRealSec();

	while(!queue.empty() && queue[0].trigger_time <= cur_time)
	{
		// Remove timer from queue before calling timer_event_func, in case timer_event_func wants to add a new timer to the queue, which might invalidate a reference.
		TimerQueueTimer timer = queue[0];
		
		// Pop timer from front of priority queue
		std::pop_heap(queue.begin(), queue.end(), TimerComparator()); 
		queue.pop_back();

		if(timer.timer_event_func)
			timer.timer_event_func(); // Call the function

		if(timer.repeating)
		{
			timer.trigger_time += timer.period;
			
			// Push timer into priority queue
			queue.push_back(timer);
			std::push_heap(queue.begin(), queue.end(), TimerComparator());
		}
	}
}


#if BUILD_TESTS


#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/TestUtils.h"
#include "../utils/PlatformUtils.h"
#include "../maths/PCG32.h"
#include <Timer.h>


void TimerQueue::test()
{
	conPrint("TimerQueue::test()");

	// Test with a single event
	{
		TimerQueue queue;
		std::vector<bool> event_occurred(1, false);
		queue.addTimer(/*period=*/0.0001f, /*repeating=*/false, /*timer event func=*/[&event_occurred](){ 
			testAssert(!event_occurred[0]);
			event_occurred[0] = true;
		});

		PlatformUtils::Sleep(1);
		queue.think();
		testAssert(event_occurred[0]);
	}

	// Test with a couple of events.  Test ordering of event fires.
	{
		TimerQueue queue;
		std::vector<bool> event_occurred(2, false);

		queue.addTimer(/*period=*/0.0001f, /*repeating=*/false, /*timer event func=*/[&event_occurred](){ 
			testAssert(!event_occurred[0]);
			event_occurred[0] = true;
		});

		queue.addTimer(/*period=*/0.000001f, /*repeating=*/false, /*timer event func=*/[&event_occurred](){ 
			testAssert(!event_occurred[0]); // This event should fire before event 0
			testAssert(!event_occurred[1]);
			event_occurred[1] = true;
		});

		PlatformUtils::Sleep(1);
		queue.think();

		testAssert(event_occurred[0]);
		testAssert(event_occurred[1]);
	}

	conPrint("TimerQueue::test() done");
}


#endif // BUILD_TESTS
