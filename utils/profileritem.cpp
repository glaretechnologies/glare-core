/*=====================================================================
profileritem.cpp
----------------
File created by ClassTemplate on Sat Aug 23 04:39:58 2003
Code By Nicholas Chapman.
=====================================================================*/
#include "profileritem.h"




ProfilerItem::ProfilerItem(const std::string& itemname_)
:	itemname(itemname_)
{
	total_time = 0;	
	timing = false;
}


ProfilerItem::~ProfilerItem()
{
	
}


void ProfilerItem::start()
{
	assert(!timing);

	timer.reset();

	timing = true;
}

void ProfilerItem::end()
{
	assert(timing);

	total_time += timer.getSecondsElapsed();

	timing = false;
}




