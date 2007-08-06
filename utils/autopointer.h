#ifndef __AUTOPOINTER_H__
#define __AUTOPOINTER_H__


template <class T>
class AutoPointer
{
public:
	inline AutoPointer();
	inline AutoPointer(T* p);
	inline AutoPointer(AutoPointer& rhs);


	inline ~AutoPointer();


	inline AutoPointer& operator = (AutoPointer& rhs);
	inline AutoPointer& set(T* newp);

	inline AutoPointer& operator = (T* p);

	//inline T& operator * ();
	inline T* operator -> ();

private:
	T* ptr;
};

template <class T>
AutoPointer<T>::AutoPointer<T>()
{
	ptr = NULL;
}

template <class T>
AutoPointer<T>::AutoPointer<T>(T* p)
:	ptr(p)
{}

template <class T>
AutoPointer<T>::AutoPointer<T>(AutoPointer<T>& rhs)
{
	ptr = rhs.ptr;
	rhs.ptr = NULL;
}


template <class T>
AutoPointer<T>::~AutoPointer<T>()
{
	delete ptr;
}

template <class T>
AutoPointer<T>& AutoPointer<T>::operator = (AutoPointer<T>& rhs)
{
	ptr = rhs.ptr;
	rhs.ptr = NULL;
	return *this;
}


template <class T>
AutoPointer<T>& AutoPointer<T>::operator = (T* newp)
{
	assert(!ptr);
	ptr = newp;
	return *this;
}


/*template <class T>
T& AutoPointer<T>::operator * ()
{
	return *ptr;
}*/

template <class T>
T* AutoPointer<T>::operator -> ()
{
	return ptr;
}

template <class T>
AutoPointer<T>& AutoPointer<T>::set(T* newp)
{
	ptr = newp;
	return *this;
}







#endif //__AUTOPOINTER_H__