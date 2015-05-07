/*=====================================================================
ContainerUtils.h
----------------
Copyright Glare Technologies Limited 2015 -

Contains some convenience methods for working with std::vectors, std::sets etc..
=====================================================================*/
#pragma once


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


// Does the set 's' contain element 'elem'?
template <typename T, typename Compare, typename Alloc>
bool contains(const std::set<T, Compare, Alloc>& s, const T& elem)
{
	return s.find(elem) != s.end();
}


} // End namespace ContainerUtils
