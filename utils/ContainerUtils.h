/*=====================================================================
ContainerUtils.h
----------------
Copyright Glare Technologies Limited 2021 -

Contains some convenience methods for working with std::vectors, std::sets etc..
=====================================================================*/
#pragma once


#include "AllocatorVector.h"
#include "Vector.h"
#include "../maths/mathstypes.h"
#include <vector>
#include <set>


namespace ContainerUtils
{


// Append v2 to the back of v1.
// Note that this procedure modifies v1.
template <typename T>
void append(std::vector<T>& v1, const std::vector<T>& v2)
{
	v1.insert(v1.end(), v2.begin(), v2.end());
}


// Append v2 to the back of v1.
// Note that this procedure modifies v1.
template <typename T, size_t align>
void append(glare::AllocatorVector<T, align>& v1, const glare::AllocatorVector<T, align>& v2)
{
	const size_t write_i = v1.size();
	const size_t v2_size = v2.size();
	v1.resize(write_i + v2_size);
	for(size_t i=0; i<v2_size; ++i)
		v1[write_i + i] = v2[i];
}


// Append v2 to the back of v1.
// Note that this procedure modifies v1.
template <typename T, size_t align>
void append(js::Vector<T, align>& v1, const js::Vector<T, align>& v2)
{
	const size_t write_i = v1.size();
	const size_t v2_size = v2.size();
	v1.resize(write_i + v2_size);
	for(size_t i=0; i<v2_size; ++i)
		v1[write_i + i] = v2[i];
}


// Append 'size' elements at data to the back of vec.
// Note that this procedure modifies vec.
template <typename T>
void append(std::vector<T>& vec, const T* data, const size_t size)
{
	const size_t write_i = vec.size();
	vec.resize(write_i + size);
	for(size_t i=0; i<size; ++i)
		vec[write_i + i] = data[i];
}


// Does vector 'v' contain element 'target'?
template <typename T>
bool contains(const std::vector<T>& v, T target)
{
	const size_t sz = v.size();
	for(size_t i=0; i<sz; ++i)
		if(v[i] == target)
			return true;
	return false;
}


// Get the length of the longest common prefix of the two vectors.
template <typename T>
size_t longestCommonPrefixLength(const std::vector<T>& a, const std::vector<T>& b)
{
	const size_t min_len = myMin(a.size(), b.size());
	size_t pre_len = 0;
	for(; pre_len < min_len && (a[pre_len] == b[pre_len]); ++pre_len)
	{}
	return pre_len;
}


// Get the tail elements (every element apart from the first) from a vector.  Requires vector to be non-empty.
template <typename T>
std::vector<T> tail(const std::vector<T>& v)
{
	assert(!v.empty());
	return std::vector<T>(v.begin() + 1, v.end());
}


// Does the set 's' contain element 'elem'?
template <typename T, typename Compare, typename Alloc>
bool contains(const std::set<T, Compare, Alloc>& s, const T& elem)
{
	return s.find(elem) != s.end();
}


// Removes the first occurrence (if present) of the element target from the vector. Returns true if it was in the vector, false otherwise.
template <typename T>
bool removeFirst(std::vector<T>& v, const T& target)
{
	const size_t sz = v.size();
	for(size_t i=0; i<sz; ++i)
		if(v[i] == target)
		{
			v.erase(v.begin() + i);
			return true;
		}
	return false;
}


// Remove first n items from vector, copy remaining data to front of buffer
template <class T>
static void removeNItemsFromFront(std::vector<T>& vec, size_t n)
{
	assert(n <= vec.size());

	const size_t vec_size = vec.size();

	for(size_t i=n; i<vec_size; ++i)
		vec[i - n] = vec[i];

	vec.resize(vec_size - n);
}


//template <typename T, typename Compare, typename Alloc>
//std::vector<T> setToVector(const std::set<T, Compare, Alloc>& s)
//{
//	std::vector<T> v;
//	v.reserve(s.size());
//	for(std::set<T, Compare, Alloc>::iterator i=s.begin(); i != s.end(); ++i)
//		v.push_back(*i);
//	return v;
//}


} // End namespace ContainerUtils
