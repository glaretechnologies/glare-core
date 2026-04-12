/*=====================================================================
RateLimitedSender.h
-------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include <functional>
class TimerQueue;


/*=====================================================================
RateLimitedSender
-----------------
For sending messages (via send_func) at a maximum rate (defined by a minimum send period).
=====================================================================*/
class RateLimitedSender
{
public:
	// send_period is minimum time elapsed between sends (calls of send_func).
	RateLimitedSender(double send_period_);
	~RateLimitedSender();


	void checkSend(TimerQueue& timer_queue, const std::function<void()>& send_func);


	static void test();

private:
	double send_period; // Minimum time elapsed between sends (calls of send_func).
	double last_send_time;
	bool send_scheduled;
};
