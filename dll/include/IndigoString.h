/*=====================================================================
IndigoString.h
--------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "IndigoVector.h"
#include <cstring>


namespace Indigo
{


///
/// A string class.
/// This is a Unicode string class.  The internal encoding is UTF-8.
/// It has a similar interface to std::string.
/// The main difference is that there is no c_str() method, as the internal buffer is not null-terminated.
///
/// If you need to convert from/to a std::string, you can do it like this:
///
/// inline const std::string toStdString(const Indigo::String& s)
/// {
///		return std::string(s.dataPtr(), s.length());
/// }
///
/// inline const Indigo::String toIndigoString(const std::string& s)
/// {
/// 	return Indigo::String(s.c_str(), s.length());
/// }
///
class String
{
public:
	USE_INDIGO_SDK_ALLOCATOR

	/// Initialise as the empty string.
	inline String();
	/// Copy-constructor
	inline String(const String& other);
	/// Initialise from a null-terminated c string.
	inline String(const char* s);
	/// Initialise with the first n bytes of s.
	explicit inline String(const char* s, size_t n);
	/// Initialise with n copies of c.
	explicit inline String(size_t n, char c);
	inline ~String();

	inline String& operator = (const String& other);

	inline String& operator += (const String& other);
	inline String& operator += (const char* other);

	inline bool operator == (const String& other) const;
	inline bool operator == (const char* other) const;
	inline bool operator != (const String& other) const;

	inline bool empty() const { return size_ == 0; }

	/// Get number of bytes in the string
	inline size_t length() const { return size_; }

	/// Get number of bytes in the string
	inline size_t size() const { return size_; }

	/// Get number of bytes allocated for this string currently.
	inline size_t capacity() const { return capacity_; }

	inline void resize(size_t newsize);
	inline void reserve(size_t capacity);

	/// Append a single character to the string.
	inline void push_back(char c);

	inline char& operator [] (size_t i) { return data[i]; }
	inline char operator [] (size_t i) const { return data[i]; }

	friend const String operator + (const String& lhs, const String& rhs);

	/// Returns a pointer to a buffer that is *not* null-terminated.
	/// Returned pointer may be NULL if the string is empty.
	inline const char* dataPtr() const { return data; } 

	/// Returns a new Vector object, containing a null-terminated character buffer.
	inline const Vector<char> getNullTerminatedBuffer() const;

	/// @private
	static void test();

private:
	static const size_t BUFSIZE = 16;

	inline bool storingOnHeap() const { return capacity_ > BUFSIZE; }

	char* data;
	char buf[BUFSIZE];
	size_t size_;
	size_t capacity_;
};


String::String()
:	data(buf), size_(0), capacity_(BUFSIZE)
{
}


String::String(const String& other)
{
	if(other.size_ <= BUFSIZE)
	{
		data = buf;

		// Copy elements over from other
		if(other.size_ > 0)
			std::memcpy(data, other.data, other.size_);

		size_ = other.size_;
		capacity_ = BUFSIZE;
	}
	else // Else we don't have the capacity to store (and will need to store new elements on the heap):
	{
		data = (char*)indigoSDKAlloc(other.size_);

		// Copy elements over from other
		std::memcpy(data, other.data, other.size_);

		size_ = other.size_;
		capacity_ = other.size_;
	}
}


String::String(const char* s)
{
	size_ = std::strlen(s);
	if(size_ == 0)
	{
		data = buf;
		capacity_ = BUFSIZE;
	}
	else if(size_ <= BUFSIZE)
	{
		data = buf;
		capacity_ = BUFSIZE;
		std::memcpy(buf, s, sizeof(char) * size_);
	}
	else
	{
		data = (char*)indigoSDKAlloc(size_);
		capacity_ = size_;
		std::memcpy(data, s, sizeof(char) * size_);
	}
}


String::String(const char* s, size_t n)
{
	size_ = n;
	if(size_ == 0)
	{
		data = buf;
		capacity_ = BUFSIZE;
	}
	else if(size_ <= BUFSIZE)
	{
		data = buf;
		capacity_ = BUFSIZE;
		std::memcpy(buf, s, sizeof(char) * size_);
	}
	else
	{
		data = (char*)indigoSDKAlloc(size_);
		capacity_ = size_;
		std::memcpy(data, s, sizeof(char) * size_);
	}
}


String::String(size_t n, char c)
{
	size_ = n;
	if(size_ == 0)
	{
		data = buf;
		capacity_ = BUFSIZE;
	}
	else if(size_ <= BUFSIZE)
	{
		data = buf;
		capacity_ = BUFSIZE;
		for(size_t i=0; i<n; ++i)
			buf[i] = c;
	}
	else
	{
		data = (char*)indigoSDKAlloc(size_);
		capacity_ = size_;
		for(size_t i=0; i<n; ++i)
			data[i] = c;
	}
}


String::~String()
{
	if(storingOnHeap())
		indigoSDKFree(data);
}


String& String::operator = (const String& other)
{
	if(this == &other)
		return *this;

	if(other.size_ <= capacity_)
	{
		// Copy elements over from other
		if(other.size_ > 0)
			std::memcpy(data, other.data, other.size_);

		size_ = other.size_;
	}
	else // Else we don't have the capacity to store (and will need to store new elements on the heap):
	{
		assert(other.size_ > BUFSIZE); // other.size must be > BUFSIZE, otherwise we would have had capacity for it (as capacity is always >= BUFSIZE).

		if(storingOnHeap())
			indigoSDKFree(data); // Free existing mem

		// Allocate new memory
		data = (char*)indigoSDKAlloc(other.size_);

		// Copy elements over from other
		std::memcpy(data, other.data, other.size_);

		size_ = other.size_;
		capacity_ = other.size_;
	}
	return *this;
}


String& String::operator += (const String& other)
{
	const size_t old_size = size();
	resize(old_size + other.size());

	if(other.size() > 0)
		std::memcpy(&data[old_size], other.data, other.size());

	return *this;
}


String& String::operator += (const char* other)
{
	const size_t other_len = ::strlen(other);
	const size_t original_size = size_;
	resize(original_size + other_len);

	for(size_t dest_i = original_size, i=0; i<other_len; ++i, ++dest_i)
		data[dest_i] = other[i];

	return *this;
}


bool String::operator == (const String& other) const
{
	if(size_ != other.size_)
		return false;
	for(size_t i=0; i<size_; ++i)
		if(data[i] != other.data[i])
			return false;
	return true;
}


bool String::operator == (const char* other) const
{
	const size_t other_size = std::strlen(other);
	if(size_ != other_size)
		return false;
	for(size_t i=0; i<size_; ++i)
		if(data[i] != other[i])
			return false;
	return true;
}


bool String::operator != (const String& other) const
{
	if(size_ != other.size_)
		return true;
	for(size_t i=0; i<size_; ++i)
		if(data[i] != other.data[i])
			return true;
	return false;
}


void String::resize(size_t new_size)
{
	if(new_size > capacity_)
		reserve(new_size);

	size_ = new_size;
}


void String::reserve(size_t n)
{
	if(n > capacity_) // If need to expand capacity
	{
		// Allocate new memory
		char* new_data = (char*)indigoSDKAlloc(n);

		// Copy-construct new objects from existing objects.
		// e[0] to e[size_-1] will now be proper initialised objects.
		if(size_ > 0)
			std::memcpy(new_data, data, size_);

		if(storingOnHeap())
			indigoSDKFree(data); // Free old buffer.

		data = new_data;
		capacity_ = n;
	}
}


void String::push_back(char c)
{
	// Check to see if we are out of capacity:
	if(size_ == capacity_)
		reserve(2 * capacity_);

	data[size_++] = c;
}


const Vector<char> String::getNullTerminatedBuffer() const
{
	Vector<char> buf_out(this->size() + 1);
	if(this->size() > 0)
		std::memcpy(&buf_out[0], data, sizeof(char) * this->size());
	buf_out[this->size()] = 0; // Null terminate buffer.
	return buf_out;
}


inline const String operator + (const String& lhs, const String& rhs)
{
	String res;
	res.resize(lhs.size() + rhs.size());
	if(lhs.size() > 0)
		std::memcpy(&res.data[0],					&lhs.data[0], sizeof(char) * (lhs.size()));
	if(rhs.size() > 0)
		std::memcpy(&res.data[0] + lhs.size(),		&rhs.data[0], sizeof(char) * (rhs.size()));
	return res;
}


inline bool operator < (const String& lhs, const String& rhs)
{
	const size_t min_size = (lhs.size() < rhs.size()) ? lhs.size() : rhs.size();
	for(size_t i = 0; i < min_size; ++i)
	{
		if(lhs[i] < rhs[i])
			return true;
		else if(lhs[i] > rhs[i])
			return false;
	}

	// Strings are the same for min_size bytes.
	return lhs.size() < rhs.size();
}


#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4127) // Disable warning C4127: conditional expression is constant
#endif
inline size_t hashFunc(const String& s)
{
	// Implemented using FNV-1a hash.
	if(sizeof(size_t) == 8)
	{
		size_t hash = 14695981039346656037ULL;
		for(size_t i=0; i<s.size(); ++i)
			hash = (hash ^ s.dataPtr()[i]) * 1099511628211ULL;
		return hash;
	}
	else
	{
		size_t hash = 2166136261U;
		for(size_t i=0; i<s.size(); ++i)
			hash = (hash ^ s.dataPtr()[i]) * 16777619U;
		return hash;
	}
}
#ifdef _WIN32
#pragma warning(pop)
#endif


} // end of namespace Indigo
