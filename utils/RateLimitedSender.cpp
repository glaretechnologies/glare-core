/*=====================================================================
RateLimitedSender.cpp
---------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "RateLimitedSender.h"


#include "TimerQueue.h"
#include "Clock.h"
#include "StringUtils.h"
#include "ConPrint.h"
#include <cassert>


RateLimitedSender::RateLimitedSender(double send_period_)
:	send_period(send_period_), last_send_time(-1000.0), send_scheduled(false)
{}


RateLimitedSender::~RateLimitedSender()
{}


void RateLimitedSender::checkSend(TimerQueue& timer_queue, const std::function<void()>& send_func)
{
	/*
	            |<-------------------- send period ----------------->|<------------------ send period ----------------->|
	------------|----------------x--------------------x--------------|--------------------------------------------------------x------- ...
	            ^                ^                    ^              ^                                                        ^
	            |                |                    |               \                                                       |
         last send time       update_1           update_2             next_gear_item_update_send_time                      update_3


	In this example, update_1 is within send_period of the last send, so instead it schedules a future send at last_send_time + send_period. (setting send_scheduled = true)

	When update_2 occurs, a future send is already scheduled (send_scheduled == true), so there is nothing to do for it.
	
	Update_3 can be sent immediately because there is no future send scheduled and it is more than send_period since last send.

	*/
	// If a send is scheduled in the future, just wait for it, so nothing to do.
	if(!send_scheduled) // If no send scheduled in the future:
	{
		const double cur_time = Clock::getCurTimeRealSec();
		const double last_send_plus_update_period = last_send_time + send_period;
		if(cur_time >= last_send_plus_update_period) // If it has been at least X seconds since last send:
		{
			// Send immediately
			// conPrint("Calling send_func immediately");
			send_func();

			last_send_time = cur_time;
		}
		else // else cur_time < last_send_plus_update_period
		{
			assert(last_send_plus_update_period > cur_time);

			// Schedule a send in the future once the update_period has elapsed since the last send.
			send_scheduled = true;

			//conPrint("Scheduling new callback in " + doubleToStringMaxNDecimalPlaces(last_send_plus_update_period - cur_time, 3) + " seconds...");

			timer_queue.addTimer(/*period=*/last_send_plus_update_period - cur_time, /*repeating=*/false, /*event func=*/[this, send_func]()
				{
					send_func();
					this->last_send_time = Clock::getCurTimeRealSec();
					this->send_scheduled = false;
				}
			);
		}
	}
}


#if BUILD_TESTS


#include "TestUtils.h"
#include "Timer.h"
#include "../maths/PCG32.h"


void RateLimitedSender::test()
{
	conPrint("RateLimitedSender::test()");

	// TODO

	conPrint("RateLimitedSender::test() done");
}


#endif // BUILD_TESTS

