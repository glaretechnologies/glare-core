/*=====================================================================
profiler.cpp
------------
File created by ClassTemplate on Sat Aug 23 04:35:35 2003
Code By Nicholas Chapman.
=====================================================================*/
#include "profiler.h"

#include "profileritem.h"
#include <vector>
#include <algorithm>

Profiler::Profiler()
{

}


Profiler::~Profiler()
{
	for(ITEM_MAP::iterator i = items.begin(); i != items.end(); ++i)
		delete (*i).second;
}


void Profiler::reset()
{
	for(ITEM_MAP::iterator i = items.begin(); i != items.end(); ++i)
		delete (*i).second;

	items.clear();

	profiler_lifetimer.reset();
}


void Profiler::startItem(const std::string& item)
{
	ITEM_MAP::iterator result = items.find(item);

	if(result == items.end())
	{
		//-----------------------------------------------------------------
		//create new item, insert
		//-----------------------------------------------------------------
		ProfilerItem* newitem = new ProfilerItem(item);
		items.insert(ITEM_MAP::value_type(item, newitem));
		
		newitem->start();		

	}
	else
	{
		(*result).second->start();
	}


}

void Profiler::endItem(const std::string& item)
{
	ITEM_MAP::iterator result = items.find(item);

	if(result == items.end())
	{
		//assert(0);
	}
	else
	{
		(*result).second->end();
	}

}


class ItemSortPred 
{
public:
	// less than operator
	bool operator () (const ProfilerItem* a, const ProfilerItem* b)
	{
		return a->getTotalTime() > b->getTotalTime();
	}
};


void Profiler::printResults(std::ostream& stream) const
{

	//-----------------------------------------------------------------
	//copy pointers to a vector
	//-----------------------------------------------------------------
	std::vector<ProfilerItem*> itemvec;
	for(ITEM_MAP::const_iterator i = items.begin(); i != items.end(); ++i)
		itemvec.push_back((*i).second);

	//-----------------------------------------------------------------
	//sort vector by item time
	//-----------------------------------------------------------------
	std::sort(itemvec.begin(), itemvec.end(), ItemSortPred());


	const PROFILER_REAL_TYPE profiler_lifetime = profiler_lifetimer.getSecondsElapsed();
	if(profiler_lifetime == 0)
		return;

	stream << "total time: " << profiler_lifetime << " secs." << std::endl;

	for(int z=0; z<(int)itemvec.size(); ++z)
	{
		ProfilerItem& item = *itemvec[z];

		const PROFILER_REAL_TYPE runfrac = item.getTotalTime() / profiler_lifetime;

		assert(runfrac >= 0 && runfrac <= 1);

		stream << item.getItemName() << ": ";
		
		//-----------------------------------------------------------------
		//print some spaces
		//-----------------------------------------------------------------
		for(int z=(int)item.getItemName().size(); z<40; ++z)
			stream << " ";

		stream.precision(3);
		stream << item.getTotalTime() << " secs   ";
		stream.precision(3);
		stream << runfrac * 100 << " percent" << std::endl;
	}
	/*for(ITEM_MAP::const_iterator i = items.begin(); i != items.end(); ++i)
	{
		ProfilerItem& item = *((*i).second);

		const PROFILER_REAL_TYPE runfrac = item.getTotalTime() / profiler_lifetime;

		assert(runfrac >= 0 && runfrac <= 1);

		stream << (*i).first << ": ";
		
		//-----------------------------------------------------------------
		//print some spaces
		//-----------------------------------------------------------------
		for(int z=(*i).first.size(); z<40; ++z)
			stream << " ";

		stream.precision(3);
		stream << item.getTotalTime() << " secs   ";
		stream.precision(3);
		stream << runfrac * 100 << " percent" << std::endl;
	}*/
}

