/*=====================================================================
WeakReference.h
---------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
// Not using pragma once since we copy this file and pragma once is path based
#ifndef GLARE_WEAKREFERENCE_H
#define GLARE_WEAKREFERENCE_H


#include "WeakRefCounted.h"


/*=====================================================================
WeakReference
-------------
A non-owning (weak) reference to a reference-counted object.
Referenced object will be automatically deleted when no strong refs to it remain.
T should be a subclass of WeakRefCounted or implement the same interface.

A weak reference does not prevent the object being destroyed, so before
accessing the object, the control block is checked to see if the object is still alive.
Use getPtrIfAlive() or upgradeToStrongRef() to do this check.

Similar to std::weak_ptr.
=====================================================================*/
template <class T>
class WeakReference
{
public:
	/// Initialises to a null reference.
	WeakReference()
	:	ob(0),
		control_block(0)
	{}

	/// Initialise as a reference to ob.
	WeakReference(T* ob_)
	{
		ob = ob_;
		if(ob)
			control_block = ob->control_block;
	}

	template<class T2>
	WeakReference(const WeakReference<T2>& other)
	{
		ob = other.getPointer();
		control_block = other.control_block;
	}

	WeakReference(const WeakReference<T>& other)
	{
		ob = other.ob;
		control_block = other.control_block;
	}

	WeakReference(const Reference<T>& other)
	{
		ob = other.ptr();

		if(ob)
			control_block = ob->control_block;
	}

	~WeakReference()
	{
	}

	T* getPtrIfAlive()
	{
		if(control_block->ob_is_alive)
			return ob;
		else
			return NULL; // Return NULL ref
	}

	Reference<T> upgradeToStrongRef()
	{
		if(control_block->ob_is_alive)
			return Reference<T>(ob);
		else
			return Reference<T>(); // Return NULL ref
	}
	
//private:
	T* ob;
	Reference<WeakRefCountedControlBlock> control_block;
};


#endif // GLARE_WEAKREFERENCE_H
