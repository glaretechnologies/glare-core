/*=====================================================================
string_view.h
-------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#if __cplusplus >= 201703L // If c++ version is >= c++17:


#include <string_view>
#include <string>


using string_view = std::string_view; // Use the standard library string_view.


inline const std::string toString(const string_view& view)
{
	return std::string(view);
}


#else // else if c++ version is < c++17:


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

	inline char operator [] (size_t index) const;

	inline string_view substr(size_t pos = 0, size_t n = std::string::npos) const;

	inline const char* data() const { return data_; }
	inline size_t size() const { return size_; }
	inline size_t length() const { return size_; }

	inline bool empty() const { return size_ == 0; }

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


char string_view::operator [] (size_t index) const
{
	assert(index < size_);
	return data_[index];
}


string_view string_view::substr(size_t pos, size_t n) const
{
	assert(pos <= size_);
	const size_t remaining = size_ - pos;
	return string_view(data() + pos, (n < remaining) ? n : remaining/*std::min(n, size() - pos)*/); // Avoid including algorithm or mathstypes just for min().
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


inline const std::string operator + (string_view lhs, string_view rhs)
{
	std::string res;
	res.resize(lhs.size() + rhs.size());
	for(size_t i=0; i<lhs.size(); ++i)
		res[i] = lhs[i];
	for(size_t i=0; i<rhs.size(); ++i)
		res[lhs.size() + i] = rhs[i];
	return res;
}


inline const std::string toString(const string_view& view)
{
	return std::string(view.data_, view.size_);
}


#endif // end if c++ version is < c++17
