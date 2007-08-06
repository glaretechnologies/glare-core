/*=====================================================================
countedhandle.h
---------------
File created by ClassTemplate on Thu Oct 28 02:34:45 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __COUNTEDHANDLE_H_666_
#define __COUNTEDHANDLE_H_666_


#include <assert.h>



/*=====================================================================
CountedHandle
-------------
handle to a reference counted object.
Does NOT automatically delete.
=====================================================================*/
template <class T>
class CountedHandle
{
public:
	/*=====================================================================
	CountedHandle
	-------------
	
	=====================================================================*/
	CountedHandle(T* ob_)
	{
		ob = ob_;

		if(ob)
			ob->incRefCount();
	}

	~CountedHandle()
	{
		if(ob)
			ob->decRefCount();
	}

	CountedHandle(const CountedHandle<T>& other)
	{
		ob = other.ob;

		if(ob)
			ob->incRefCount();
	}

	CountedHandle& operator = (const CountedHandle& other)
	{
		if(&other == this)//assigning a reference object to itself
			return *this;

		//-----------------------------------------------------------------
		//dec old ref that this object used to contain
		//-----------------------------------------------------------------
		if(ob)
			ob->decRefCount();

		ob = other.ob;

		if(ob)
			ob->incRefCount();

		return *this;
	}

	//equality is defined as equality of the pointed to objects
	bool operator == (const CountedHandle& other) const
	{
		return *ob == *other.ob;
	}

	//less than is defined as less than for the pointed to objects
	bool operator < (const CountedHandle& other) const
	{
		return *ob < *other.ob;
	}


	T& operator * ()
	{
		assert(ob);
		return *ob;
	}

	T* operator -> ()
	{
		assert(ob);
		return ob;
	}

	const T& operator * () const
	{
		assert(ob);
		return *ob;
	}

	const T* operator -> () const
	{
		assert(ob);
		return ob;
	}

	//operator bool is a BAD IDEA. cause mystream thinks refs are bools.
	/*operator bool() const
	{
		return ob != NULL;
	}*/

	bool isNull() const
	{
		return ob == NULL;
	}

	//NOTE: this method is dangerous
	T* getPointer()
	{
		return ob;
	}
	
	const T* getPointer() const
	{
		return ob;
	}

private:
	T* ob;
};




#endif //__COUNTEDHANDLE_H_666_




