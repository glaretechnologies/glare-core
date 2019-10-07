/*=====================================================================
ContainerUtils.h
----------------
Copyright Glare Technologies Limited 2015 -

Contains some convenience methods for working with std::vectors, std::sets etc..
=====================================================================*/
#pragma once


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
