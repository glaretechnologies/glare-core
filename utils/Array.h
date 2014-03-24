/*=====================================================================
Array.cpp
-------------------
Copyright Glare Technologies Limited 2014 -
=====================================================================*/
#pragma once


#include <assert.h>
#include <algorithm>


/*======================================================================================
Array
-----
1d array with size member.
operator[] is bound-checked with assert()
======================================================================================*/
template <class Value>
class Array
{
public:
	//inline Array();
	inline Array();
	inline Array(int size);
	inline Array(const Value* data, int datasize);//copies data
	inline Array(const Array& rhs);
	inline ~Array();

	inline Array& operator = (const Array& rhs);

	inline Value& operator[] (int index);
	inline const Value& operator[] (int index) const;

	inline int size() const { return siz; }

	//inline Value* data() { return data; }

	inline void resize(int newsize);

	//-----------------------------------------------------------------
	//iterator stuff
	//-----------------------------------------------------------------
	typedef Value* iterator;
	typedef const Value* const_iterator;

	inline iterator begin(){ return data; }
	inline iterator end(){ return data + siz; }//NOTE: optimise this?

	inline const_iterator begin() const { return data; }
	inline const_iterator end()const { return data + siz; }

private:
	Value* data;
	int siz;
};


template <class Value>
Array<Value>::Array()
:	siz(0),
	data(NULL)
{
}


template <class Value>
Array<Value>::Array(const Value* data_, int datasize)
:	siz(datasize)
{
	data = new Value[datasize];
	for(int i=0; i<datasize; ++i)
		data[i] = data_[i];
}

template <class Value>
Array<Value>::Array(int size_)
:	siz(size_)
{
	data = new Value[siz];
}

template <class Value>
Array<Value>::Array(const Array& rhs)
{
	if(siz == rhs.siz)
	{
		//-----------------------------------------------------------------
		//no need to resize data, just copy contents
		//-----------------------------------------------------------------
		for(int i=0; i<siz; ++i)
		{
			data[i] = rhs.data[i];
		}
	}
	else
	{
		delete[] data;
		siz = rhs.siz;
		data = new Value[siz];

		for(int i=0; i<siz; ++i)
		{
			data[i] = rhs.data[i];
		}
	}
}


template <class Value>
Array<Value>::~Array()
{
	delete[] data;
}

template <class Value>
Array<Value>& Array<Value>::operator = (const Array& rhs)
{
	if(&rhs == this)
		return;

	if(siz == rhs.siz)
	{
		//-----------------------------------------------------------------
		//no need to resize data, just copy contents
		//-----------------------------------------------------------------
		for(int i=0; i<siz; ++i)
		{
			data[i] = rhs.data[i];
		}
	}
	else
	{
		delete[] data;
		siz = rhs.siz;
		data = new Value[siz];

		for(int i=0; i<siz; ++i)
		{
			data[i] = rhs.data[i];
		}
	}


	return *this;
}

template <class Value>
void Array<Value>::resize(int newsize)
{
	if(newsize == siz)
		return;

	Value* newdata = new Value[newsize];

	int minsize;
	if(newsize < siz)
		minsize = newsize;
	else
		minsize = siz;

	for(int i=0; i<minsize; i++)
	{
		newdata[i] = data[i];
	}

	delete[] data;
	data = newdata;

	siz = newsize;
}



template <class Value>
Value& Array<Value>::operator[] (int index)
{
	assert(index >= 0 && index < siz);
	return data[index];
}



template <class Value>
const Value& Array<Value>::operator[] (int index) const
{
	assert(index >= 0 && index < siz);
	return data[index];
}
