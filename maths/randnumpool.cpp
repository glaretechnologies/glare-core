/*=====================================================================
randnumpool.cpp
---------------
File created by ClassTemplate on Tue Mar 01 19:04:56 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "randnumpool.h"

#include "../utils/random.h"



RandNumPool::RandNumPool(int size_)
{
	size = size_;
	sizemask = size - 1;
	next = 0;

	nums.resize(size);

	for(unsigned int i=0; i<size; ++i)
		nums[i] = Random::unit();
}


RandNumPool::~RandNumPool()
{
	
}






