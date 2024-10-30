/*=====================================================================
ArrayRef.h
----------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "SmallArray.h"
#include "SmallVector.h"
#include "Array.h"
#include "Vector.h"
#include "AllocatorVector.h"
#include "RuntimeCheck.h"
#include <vector>
#include <assert.h>


/*======================================================================================
ArrayRef
--------
A (pointer, size) pair.
Offers a read-only view on a contiguous array of values.
Idea based on LLVM's ArrayRef.

Use MutableArrayRef (defined below) for a read/write view.
======================================================================================*/
template <class T>
class ArrayRef
{
public:
	inline ArrayRef(const T* data, size_t len);

	template <int N>
	inline ArrayRef(const SmallVector<T, N>& v) : data_(v.data()), len(v.size()) {}

	template <int N>
	inline ArrayRef(const SmallArray<T, N>& a)  : data_(a.data()), len(a.size()) {}

	inline ArrayRef(const std::vector<T>& v)    : data_(v.data()), len(v.size()) {}

	inline ArrayRef(const glare::Array<T>& a)   : data_(a.data()), len(a.size()) {}

	template <size_t A>
	inline ArrayRef(const js::Vector<T, A>& a)  : data_(a.data()), len(a.size()) {}

	template <size_t A>
	inline ArrayRef(const glare::AllocatorVector<T, A>& a)  : data_(a.data()), len(a.size()) {}

	inline const T& operator[] (size_t index) const;

	inline size_t size() const { return len; }

	inline size_t dataSizeBytes() const { return len * sizeof(T); }
	
	inline const T* data() const { return data_; }

	inline bool empty() const { return len == 0; }

	inline const T& back() const { assert(len >= 1); return data_[len - 1]; }

	inline ArrayRef<T> getSlice(size_t slice_offset, size_t slice_len) const;

	// Does runtime checks to see if the slice is in bounds, throws glare::Exception if not.
	inline ArrayRef<T> getSliceChecked(size_t slice_offset, size_t slice_len) const;

	//-----------------------------------------------------------------
	// iterator stuff
	//-----------------------------------------------------------------
	typedef const T* const_iterator;

	inline const_iterator begin() const { return data_; }
	inline const_iterator end() const { return data_ + len; }

	

protected:
	const T* data_;
	size_t len;
};


void doArrayRefTests();


template <class T>
ArrayRef<T>::ArrayRef(const T* newdata, size_t len_)
:	data_(newdata), len(len_)
{}


template <class T>
const T& ArrayRef<T>::operator[] (size_t index) const
{
	assert(index < len);
	return data_[index];
}


template<class T>
ArrayRef<T> ArrayRef<T>::getSlice(size_t slice_offset, size_t slice_len) const
{
	return ArrayRef<T>(data_ + slice_offset, slice_len);
}


template<class T>
ArrayRef<T> ArrayRef<T>::getSliceChecked(size_t slice_offset, size_t slice_len) const
{
	runtimeCheck(
		!Maths::unsignedIntAdditionWraps(slice_offset, slice_len) && 
		((slice_offset + slice_len) <= len)
	);
	return ArrayRef<T>(data_ + slice_offset, slice_len);
}


/*======================================================================================
MutableArrayRef
---------------
A (pointer, size) pair.
Offers a read/write view on a contiguous array of values.
Idea based on LLVM's MutableArrayRef.

Make it inherit from ArrayRef so that we can pass a MutableArrayRef to a function taking ArrayRef parameters.
(This makes sense as something providing read/write can also provide read-only access)
======================================================================================*/
template <class T>
class MutableArrayRef : public ArrayRef<T>
{
public:
	inline MutableArrayRef(T* data, size_t len);

	inline MutableArrayRef(std::vector<T>& v)    : ArrayRef<T>(v.data(), v.size()) {}

	inline T& operator[] (size_t index);
	
	inline T* data() const { return const_cast<T*>(ArrayRef<T>::data_); }

	// Does runtime checks to see if the slice is in bounds, throws glare::Exception if not.
	inline MutableArrayRef<T> getSliceChecked(size_t slice_offset, size_t slice_len) const;

	//-----------------------------------------------------------------
	// iterator stuff
	//-----------------------------------------------------------------
	typedef T* iterator;

	inline iterator begin() { return data(); }
	inline iterator end() { return data() + ArrayRef<T>::len; }
};


template <class T>
MutableArrayRef<T>::MutableArrayRef(T* newdata, size_t len_)
:	ArrayRef<T>(newdata, len_)
{}


template <class T>
T& MutableArrayRef<T>::operator[] (size_t index)
{
	assert(index < ArrayRef<T>::len);
	return (const_cast<T*>(ArrayRef<T>::data_))[index];
}


template<class T>
MutableArrayRef<T> MutableArrayRef<T>::getSliceChecked(size_t slice_offset, size_t slice_len) const
{
	runtimeCheck(
		!Maths::unsignedIntAdditionWraps(slice_offset, slice_len) && 
		((slice_offset + slice_len) <= ArrayRef<T>::len)
	);
	return MutableArrayRef<T>(data() + slice_offset, slice_len);
}


template <class T>
inline bool areDisjoint(ArrayRef<T> a, ArrayRef<T> b)
{
	return (a.end() <= b.begin()) || (b.end() <= a.begin());
}


void checkedArrayRefMemcpy(MutableArrayRef<uint8> dest, size_t dest_start_index, ArrayRef<uint8> src, size_t src_start_index, size_t size_B);
