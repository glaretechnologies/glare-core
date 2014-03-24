/*=====================================================================
IndigoAtomic.cpp
----------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Nov 17 12:56:10 +1300 2010
=====================================================================*/
#include "IndigoAtomic.h"


#include "Lock.h"



IndigoAtomic::IndigoAtomic(int val_)
:	val(val_)
{

}


IndigoAtomic::~IndigoAtomic()
{

}


IndigoAtomic::operator int()// const
{
	Lock lock(mutex);

	return val;
}


IndigoAtomic& IndigoAtomic::operator =(int val_)
{
	Lock lock(mutex);

	val = val_;

	return *this;
}


int IndigoAtomic::fetchAndIncrement(int incr_val)
{
	Lock lock(mutex);

	int old_val = val;
	val += incr_val;

	return old_val;
}
