#ifndef __CIRCULARARRAY_H_666_
#define __CIRCULARARRAY_H_666_

#include "array.h"
#include <assert.h>
#include <vector>



//class CircArrayIter;




/*=====================================================================
CircularArray
-------------
=====================================================================*/
template <class Value>
class CircularArray
{
public:
	/*=====================================================================
	CircularArray
	-------------
	=====================================================================*/
	CircularArray(int size);

	~CircularArray();


	inline int getIndex() const { return index; }

	inline int getSize() const { return data.size(); }

	//write and advance index
	inline void write(const Value& newval)
	{
		data[index] = newval;

		index = (index + 1) % data.size();
	}

	inline Value& getElem(int index)
	{
		return data[index];
	}
	
	inline const Value& getElem(int index) const
	{
		return data[index];
	}



	inline Value& indexElem()
	{
		return getElem(index);
	}
	inline const Value& indexElem() const
	{
		return getElem(index);
	}



	inline Value& elemBefore(int back_offset)
	{
		assert(back_offset < getSize());

		int backindex = index - back_offset;

		//NEWCODE: changed this from if to while
		while(backindex < 0)
			backindex += getSize();

		return getElem(backindex);
	}

	inline const Value& elemBefore(int back_offset) const
	{
		assert(back_offset < getSize());

		int backindex = index - back_offset;

		while(backindex < 0)
			backindex += getSize();

		return getElem(backindex);
	}


	
	
	//-----------------------------------------------------------------
	//iterator stuff
	//-----------------------------------------------------------------
//	typedef CircArrayIter* iterator;
	//typedef int const_iterator;

//	inline iterator begin(){ CircArrayIter(index); }
//	inline iterator end(){ CircArrayIter(index + 1); }

	//inline const_iterator begin() const { return index; }
	//inline const_iterator end()const { return index + 1 }

private:
	int index;
	Array<Value> data;

};


template <class Value>
class CircArrayIter
{
public:
	CircArrayIter(int index, CircularArray<Value>* array);
	~CircArrayIter();

	CircArrayIter& operator ++ ();
	Value& operator * ();
	bool operator == (const CircArrayIter& rhs) const;
	CircArrayIter& operator = (const CircArrayIter& rhs);

private:
	int index;
	CircularArray<Value>* array;
};




template <class Value>
CircularArray<Value>::CircularArray(int size)
:	data(size)
{
	index = 0;
	
}

template <class Value>
CircularArray<Value>::~CircularArray()
{
	
}





















template <class Value>
CircArrayIter<Value>::CircArrayIter(int index_, CircularArray<Value>* array_)
{
	index = index_;
	array = array_;
}

template <class Value>
CircArrayIter<Value>::~CircArrayIter(){}

template <class Value>
CircArrayIter<Value>& CircArrayIter<Value>::operator ++ ()
{
	index++;
}

template <class Value>
Value& CircArrayIter<Value>::operator * ()
{
	return array->getElem(index);
}

template <class Value>
bool CircArrayIter<Value>::operator == (const CircArrayIter<Value>& rhs) const
{
	assert(array == rhs.array);

	return index == rhs.index;
}

template <class Value>
CircArrayIter<Value>& CircArrayIter<Value>::operator = (const CircArrayIter<Value>& rhs)
{
	array = rhs.array;
	index = rhs.index;
}





#endif //__CIRCULARARRAY_H_666_




