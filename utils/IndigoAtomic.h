/*=====================================================================
IndigoAtomic.h
--------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Nov 17 12:56:10 +1300 2010
=====================================================================*/
#pragma once

#include "platform.h"
#include "mutex.h"
//#include "lock.h"



/*=====================================================================
IndigoAtomic
------------

=====================================================================*/
class IndigoAtomic
{
public:
	IndigoAtomic(int val_ = 0);
	~IndigoAtomic();


	operator int();// const;
	IndigoAtomic& operator =(int val_);

	int fetchAndIncrement(int incr_val);


private:

	int val;

	Mutex mutex;
};
