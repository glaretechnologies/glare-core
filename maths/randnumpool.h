/*=====================================================================
randnumpool.h
-------------
File created by ClassTemplate on Tue Mar 01 19:04:56 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __RANDNUMPOOL_H_666_
#define __RANDNUMPOOL_H_666_


#include <vector>
gfghfg

/*=====================================================================
RandNumPool
-----------

=====================================================================*/
class RandNumPool
{
public:
	/*=====================================================================
	RandNumPool
	-----------
	size must be a power of two
	=====================================================================*/
	RandNumPool(int size);

	~RandNumPool();


	inline float nextNum();

	//inline float getNum();


private:
	std::vector<float> nums;
	unsigned int next;
	unsigned int size;
	unsigned int sizemask;
};

inline float RandNumPool::nextNum()
{
	const float x = nums[next];

	next = (next + 1) & sizemask;

	return x;
}



#endif //__RANDNUMPOOL_H_666_




