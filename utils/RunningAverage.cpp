/*=====================================================================
RunningAverage.cpp
------------------
File created by ClassTemplate on Sun Feb 19 21:23:18 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "RunningAverage.h"


#include <assert.h>

RunningAverage::RunningAverage(int history_len_)
:	history_len(history_len_)
{
	
}


RunningAverage::~RunningAverage()
{
	
}

double RunningAverage::getAv() const
{
	if(values.empty())
		return 0.0;

	double av = 0.0;
	for(unsigned int i=0; i<values.size(); ++i)
		av += values[i];
	return av / (double)values.size();
}
	
void RunningAverage::addValue(double val)
{
	assert(values.size() <= this->history_len);

	if(values.size() == this->history_len)
	{
		//shift values left
		for(int i=0; i<(int)values.size()-1; ++i)
			values[i] = values[i+1];

		values[values.size()-1] = val;
	}
	else
		values.push_back(val);
}




