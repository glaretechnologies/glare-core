/*=====================================================================
string_view.h
-------------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#pragma once


#include <string>
#include <cassert>
#include <cstring>


/*

string_view is a lightweight 'view' of part or all of a string.
It doesn't own the string, just points to one, so the string/buffer it points
to needs to be around for >= the lifetime of the string view.

string_view will probably make it into the C++ standard library at some point
(right now it's in std::experimental namespace in some standard libraries)

Once it's standard we can remove this implementation.

Roughly based on
https://llvm.org/svn/llvm-project/libcxx/trunk/include/experimental/string_view
*/
class string_view
{
public:
	inline string_view();
	inline string_view(const std::string& s);
	inline string_view(const char* c);
	inline string_view(const char* c, size_t sz);

	inline const char operator [] (size_t index) const;

	inline const char* data() const { return data_; }
	inline const size_t size() const { return size_; }

	inline const std::string to_string() const;

	const char* data_;
	size_t size_;
};


void testStringView();


string_view::string_view() : data_(0), size_(0)
{}


string_view::string_view(const std::string& s) : data_(s.data()), size_(s.size())
{}


string_view::string_view(const char* c)
	 : data_(c), size_(strlen(c))
{
}


string_view::string_view(const char* c, size_t sz)
	 : data_(c), size_(sz)
{
}


const char string_view::operator [] (size_t index) const
{
	assert(index < size_);
	return data_[index];
}


const std::string string_view::to_string() const
{
	return std::string(data_, data_ + size_);
}


//============ stand-alone ===========

inline bool operator == (string_view lhs, string_view rhs)
{
    if(lhs.size() != rhs.size()) 
		return false;
	return std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}


inline bool operator != (string_view lhs, string_view rhs)
{
    if(lhs.size() != rhs.size()) 
		return true;
	return std::memcmp(lhs.data(), rhs.data(), lhs.size()) != 0;
}

