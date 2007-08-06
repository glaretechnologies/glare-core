/*=====================================================================
bigarray.h
----------
File created by ClassTemplate on Wed Nov 24 13:07:07 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __BIGARRAY_H_666_
#define __BIGARRAY_H_666_


#include "platform.h"
#include <vector>


/*=====================================================================
BigArray
--------

=====================================================================*/
template <class T>
class BigArray
{
public:
	/*=====================================================================
	BigArray
	--------
	
	=====================================================================*/
	BigArray();

	~BigArray();

	typedef unsigned int size_type;

	void push_back(const T& t);

	T& operator [] (size_type index);
	const T& operator [] (size_type index) const;

	size_type size() const;
	bool empty() const;

	size_type capacity() const;
	void reserve(size_type minnewcapacity);


private:
	int getSubVecSize();//in num elements, depends on sizeof(T)
	int getSubVecForIndex(size_type index);
	int getOffsetForIndex(size_type index);

	std::vector<std::vector<T> > data;

	size_type size_;
	size_type capacity_;
};


template <class T>
BigArray<T>::BigArray()
{
	size_ = 0;
	capacity_ = 0;

	//data.reserve(100);
	data.resize(100);
}


template <class T>
BigArray<T>::~BigArray()
{
	
}



template <class T>
void BigArray<T>::push_back(const T& t)
{
	//size_++;

	const int subvecindex = getSubVecForIndex(size_);
	//const int subindex = getOffsetForIndex(size_);

	/*if(subvecindex >= data.size())
	{
		data.push_back(std::vector<T>());
	}*/


	(data[subvecindex]).push_back(t);

	size_++;
}

template <class T>
T& BigArray<T>::operator [] (size_type index)
{
	const int subvec = getSubVecForIndex(index);
	const int subindex = getOffsetForIndex(index);

	return (data[subvec])[subindex];
}
	
template <class T>
const T& BigArray<T>::operator [] (size_type index) const
{
	const int subvec = getSubVecForIndex(index);
	const int subindex = getOffsetForIndex(index);

	return (data[subvec])[subindex];
}

template <class T>
unsigned int BigArray<T>::size() const
{
	return size_;
}

template <class T>
bool BigArray<T>::empty() const
{
	return size_ == 0;
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
	return index / getSubVecSize();
}

template <class T>
int BigArray<T>::getOffsetForIndex(size_type index)
{
	return index % getSubVecSize();
}

template <class T>
unsigned int BigArray<T>::capacity() const
{
	return capacity_;
}

template <class T>
void BigArray<T>::reserve(size_type minnewcapacity)
{
	//NOTE: slightly incorrect: should be using index not capacity here.
	const int subvecindex = getSubVecForIndex(minnewcapacity);
	const int index_in_subvec = getOffsetForIndex(minnewcapacity);

	for(int i=0; i<subvecindex; ++i)
	{
		data[i].reserve(getSubVecSize());//reserve to full size
	}

	data[subvecindex].reserve(index_in_subvec);

}



#endif //__BIGARRAY_H_666_




