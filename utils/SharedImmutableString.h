/*=====================================================================
SharedImmutableString.h
-----------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "Reference.h"
#include "string_view.h"
#include <assert.h>
#include <string>
//#include <memory>
//#include <new>


namespace glare
{


/*=====================================================================
SharedImmutableStringData
-------------------------

=====================================================================*/
class SharedImmutableStringData : public ThreadSafeRefCounted
{
public:
	

	inline SharedImmutableStringData(); // Initialise as an empty vector.
	//inline SharedImmutableString(size_t count, const char val = '\0'); // Initialise with count copies of val.
	//inline SharedImmutableString(const char* begin, const char* end); // Range constructor
	//inline SharedImmutableString(const SharedImmutableString& other); // Initialise as a copy of other
	inline ~SharedImmutableStringData();

	//inline SharedImmutableString& operator=(const SharedImmutableString& other);

	//inline void resizeNoCopy(size_t new_size); // Resize to size N, using default constructor.

	inline size_t size() const;
	inline size_t dataSizeBytes() const;
	inline bool empty() const;
	inline bool nonEmpty() const;

	const std::string toString() const { return std::string(data(), size_); }
	string_view toStringView() const { return string_view(data(), size_); }

	inline bool operator == (const SharedImmutableStringData& other); 
	inline bool operator != (const SharedImmutableStringData& other); 

	inline int compare(const SharedImmutableStringData& other); 

	inline const char* data() const { return (const char*)(this) + sizeof(SharedImmutableStringData); }

	inline const char& back() const;
	inline const char& operator[](size_t index) const;

	typedef const char* const_iterator;

	inline const_iterator begin() const;
	inline const_iterator end() const;

	
//private:
	//char* e;
	//size_t size_; // Number of elements in the vector.   Elements e[0] to e[size_-1] are proper constructed objects.
	//SharedImmutableStringHeader header;
	uint64 size_;
	uint64 hash;
};


typedef Reference<SharedImmutableStringData> SharedImmutableStringDataRef;

[[nodiscard]] Reference<SharedImmutableStringData> makeSharedImmutableStringData(const char* data, size_t len);
[[nodiscard]] Reference<SharedImmutableStringData> makeSharedImmutableStringData(string_view str);
void doDestroySharedImmutableStringData(SharedImmutableStringData* str);




/*=====================================================================
SharedImmutableString
---------------------

=====================================================================*/
class SharedImmutableString
{
public:
	explicit SharedImmutableString(const Reference<SharedImmutableStringData>& string_) : string(string_) {}

	const size_t size() const { return string ? string->size() : 0; }
	const char* data() const { return string ? string->data() : nullptr; }

	bool operator == (const SharedImmutableString& other) const
	{
		if(string.isNull())
			return other.string.isNull();
		
		assert(string.nonNull());
		if(other.string.isNull())
			return false;

		return *string == *other.string;
	}
	bool operator != (const SharedImmutableString& other) const { return !(*this == other); }

	[[nodiscard]] string_view toStringView() const { return string ? string_view(string->data(), string->size()) : string_view(); }
	[[nodiscard]] std::string toString() const { return string ? std::string(string->data(), string->size()) : std::string(); }


	static void test();
//private:
	Reference<SharedImmutableStringData> string; // Null if this is the empty string, non-null otherwise.
};


[[nodiscard]] inline SharedImmutableString makeSharedImmutableString(const char* data, size_t len)
{ 
	return SharedImmutableString((len == 0) ? nullptr : makeSharedImmutableStringData(data, len));
}

[[nodiscard]] inline SharedImmutableString makeSharedImmutableString(string_view str)
{ 
	return SharedImmutableString((str.size() == 0) ? nullptr : makeSharedImmutableStringData(str.data(), str.size()));
}

[[nodiscard]] inline SharedImmutableString makeEmptySharedImmutableString() { return SharedImmutableString(nullptr); }





SharedImmutableStringData::SharedImmutableStringData()
:	size_(0), hash(0)
{}


//SharedImmutableString::SharedImmutableString(size_t count, const char val)
//:	size_(count)
//{
//	e = static_cast<char*>(MemAlloc::alignedSSEMalloc(sizeof(char) * count)); // Allocate new memory on heap
//
//	std::uninitialized_fill(e, e + count, val); // Construct elems
//}
//
//
//SharedImmutableString::SharedImmutableString(const char* begin_, const char* end_) // Range constructor
//:	size_(end_ - begin_)
//{
//	e = static_cast<char*>(MemAlloc::alignedSSEMalloc(sizeof(char) * size_)); // Allocate new memory on heap
//
//	std::uninitialized_copy(begin_, end_, /*dest=*/e);
//}
//
//
//SharedImmutableString::SharedImmutableString(const SharedImmutableString& other)
//{
//	e = static_cast<char*>(MemAlloc::alignedSSEMalloc(sizeof(char) * other.size_)); // Allocate new memory on heap
//
//	// Copy-construct new objects from existing objects in 'other'.
//	std::uninitialized_copy(other.e, other.e + other.size_, /*dest=*/e);
//	size_ = other.size_;
//}


SharedImmutableStringData::~SharedImmutableStringData()
{
	//MemAlloc::alignedFree(e);
}


//SharedImmutableString& SharedImmutableString::operator=(const SharedImmutableString& other)
//{
//	if(this == &other)
//		return *this;
//
//	MemAlloc::alignedFree(e); // Free existing mem
//
//	// Allocate new memory
//	e = static_cast<char*>(MemAlloc::alignedSSEMalloc(sizeof(charT) * other.size_));
//
//	// Copy elements over from other
//	std::uninitialized_copy(other.e, other.e + other.size_, e);
//
//	size_ = other.size_;
//
//	return *this;
//}


//void SharedImmutableString::resizeNoCopy(size_t new_size)
//{
//	// Allocate new memory
//	char* new_e = static_cast<char*>(MemAlloc::alignedSSEMalloc(sizeof(char) * new_size));
//
//	// Initialise elements
//	// NOTE: We use the constructor form without parentheses, in order to avoid default (zero) initialisation of POD types. 
//	// See http://stackoverflow.com/questions/620137/do-the-parentheses-after-the-type-name-make-a-difference-with-new for more info.
//	for(char* elem=new_e; elem<new_e + new_size; ++elem)
//		::new (elem) T;
//
//	if(e)
//		MemAlloc::alignedFree(e); // Free old buffer.
//
//	e = new_e;
//	size_ = new_size;
//}


size_t SharedImmutableStringData::size() const
{
	return size_;
}


size_t SharedImmutableStringData::dataSizeBytes() const
{
	return size_ * sizeof(char);
}


bool SharedImmutableStringData::empty() const
{
	return size_ == 0;
}


inline bool SharedImmutableStringData::nonEmpty() const
{
	return size_ != 0;
}


inline bool SharedImmutableStringData::operator==(const SharedImmutableStringData& other)
{
	return (hash == other.hash) && (compare(other) == 0);
}


inline bool SharedImmutableStringData::operator!=(const SharedImmutableStringData& other)
{
	return (hash != other.hash) || (compare(other) != 0);
}


inline int SharedImmutableStringData::compare(const SharedImmutableStringData& other)
{
	// TODO: use hash?
	if(size_ < other.size_)
		return -1;
	else if(size_ > other.size_)
		return 1;
	else
	{
		const char* const d = data();
		const char* const other_d = other.data();
		for(size_t i=0; i<size_; ++i)
			if(d[i] != other_d[i])
			{
				if(d[i] < other_d[i])
					return -1;
				else
					return 1;
			}

		return 0;
	}
}


const char& SharedImmutableStringData::back() const
{
	assert(size_ >= 1);
	return data()[size_ - 1];
}


const char& SharedImmutableStringData::operator[](size_t index) const
{
	assert(index < size_);
	return data()[index];
}


typename SharedImmutableStringData::const_iterator SharedImmutableStringData::begin() const
{
	return data();
}


typename SharedImmutableStringData::const_iterator SharedImmutableStringData::end() const
{
	return data() + size_;
}


//struct SharedImmutableStringRefKeyEqual
//{
//	size_t operator() (const glare::SharedImmutableStringRef& a, const glare::SharedImmutableStringRef& b) const
//	{
//		return *a == *b;
//	}
//};
//
//
//struct SharedImmutableStringRefHasher
//{
//	size_t operator() (const glare::SharedImmutableStringRef& ref) const
//	{
//		return (size_t)ref->hash;
//	}
//};


// For SharedImmutableStringHandles from a SharedStringTable, where pointer equality can be used.
struct SharedImmutableStringHandleKeyPointerEqual
{
	bool operator() (const glare::SharedImmutableString& a, const glare::SharedImmutableString& b) const
	{
		return a.string.getPointer() == b.string.getPointer();
	}
};


struct SharedImmutableStringHandleKeyValueEqual
{
	bool operator() (const glare::SharedImmutableString& a, const glare::SharedImmutableString& b) const
	{
		if(a.string.isNull() || b.string.isNull())
			return a.string.isNull() && b.string.isNull();

		return *a.string == *b.string;
	}
};


struct SharedImmutableStringHandleHasher
{
	size_t operator() (const SharedImmutableString& handle) const
	{
		return (size_t)(handle.string ? handle.string->hash : 0);
	}
};


} // end namespace glare


// References destroy objects via this function.  By default it just deletes the object, but this function can be specialised
// for specific classes to e.g. free to a memory pool.
// Template specialisation of destroyAndFreeOb for SharedImmutableString.
template <>
inline void destroyAndFreeOb(glare::SharedImmutableStringData* ob)
{
	doDestroySharedImmutableStringData(ob);
}


