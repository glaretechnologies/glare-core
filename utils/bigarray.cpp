/*=====================================================================
bigarray.cpp
------------
File created by ClassTemplate on Wed Nov 24 13:07:07 2004Code By Nicholas Chapman.
=====================================================================*/
#include "bigarray.h"


/*
template <class T>
BigArray<T>::BigArray()
{
	
}


template <class T>
BigArray<T>::~BigArray()
{
	
}



template <class T>
void BigArray<T>::push_back(const T& t)
{
	//size_++;

	const int subvec = getSubVecForIndex(size_);
	//const int subindex = getOffsetForIndex(size_);

	if(subvec + 1 < data.size())
	{
		data.push_back(std::vector<T>);
	}


	(data[subvec]).push_back(t);
}

template <class T>
T& BigArray<T>::operator [] (size_type index)
{
	const int subvec = getSubVecForIndex(size_);
	const int subindex = getOffsetForIndex(size_);

	return (data[subvec])[subindex];
}
	
template <class T>
const T& BigArray<T>::operator [] (size_type index) const
{
	const int subvec = getSubVecForIndex(size_);
	const int subindex = getOffsetForIndex(size_);

	return (data[subvec])[subindex];
}

template <class T>
BigArray<T>::size_type BigArray<T>::size() const
{
	return size;
}

const unsigned int SUBVEC_SIZE_BYTES = 500000000;//500MB

template <class T>
int BigArray<T>::getSubVecSize()//in num elements
{
	return SUBVEC_SIZE_BYTES / sizeof(T);
}


template <class T>
int BigArray<T>::getSubVecForIndex(size_type index)
{
	return index * getSubVecSize();
}

template <class T>
int BigArray<T>::getOffsetForIndex(size_type index)
{
	return index % getSubVecSize();
}
*/



