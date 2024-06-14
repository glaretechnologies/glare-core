/*=====================================================================
UniqueRef.h
-----------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


/*=====================================================================
UniqueRef
---------
Similar to std::unique_ptr without having a 4000 line header file.
=====================================================================*/
template <class T>
class UniqueRef
{
public:
	// Initialises to a null reference.
	UniqueRef()
	:	ob(0)
	{}

	// Initialise as a reference to ob.
	explicit UniqueRef(T* ob_)
	:	ob(ob_)
	{}

	UniqueRef(const UniqueRef&) = delete;
	
	~UniqueRef()
	{
		delete ob;
	}

	UniqueRef& operator = (const UniqueRef&) = delete;


	void set(T* newob)
	{
		if(newob != ob)
			delete ob;
		ob = newob;
	}

	inline T* operator -> () const
	{
		return ob;
	}
	
	inline T* ptr() const
	{
		return ob;
	}

	inline T* ptr()
	{
		return ob;
	}

private:
	T* ob;
};
