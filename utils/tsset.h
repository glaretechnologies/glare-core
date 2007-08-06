/*=====================================================================
tsset.h
-------
File created by ClassTemplate on Thu May 05 23:45:45 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TSSET_H_666_
#define __TSSET_H_666_


#include <set>
#include "mutex.h"

/*=====================================================================
TSSet
-----
Thread-safe set.
A std::set wrapped with a mutex.
=====================================================================*/
template <class T>
class TSSet
{
public:
	/*=====================================================================
	TSSet
	-----
	
	=====================================================================*/
	TSSet(){}

	~TSSet(){}

	inline Mutex& getMutex(){ return mutex; }


	std::set<T> set;
private:
	mutable Mutex mutex;
};



#endif //__TSSET_H_666_




