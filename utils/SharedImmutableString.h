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


namespace glare
{


class SharedImmutableStringData;


/*=====================================================================
SharedImmutableString
---------------------

=====================================================================*/
class SharedImmutableString
{
public:
	SharedImmutableString() {}
	explicit SharedImmutableString(const Reference<SharedImmutableStringData>& string_) : string(string_) {}

	inline const size_t size() const;
	inline const char* data() const;

	inline bool empty() const { return string.isNull(); }

	inline bool operator == (const SharedImmutableString& other) const;
	inline bool operator != (const SharedImmutableString& other) const;

	inline bool operator < (const SharedImmutableString& other) const;
	inline bool pointerLessThan(const SharedImmutableString& other) const;

	inline bool operator == (const std::string& other) const;
	inline bool operator != (const std::string& other) const;

	inline operator string_view () const { return toStringView(); } // Conversion operator

	[[nodiscard]] inline string_view toStringView() const;
	[[nodiscard]] inline std::string toString() const;

	inline size_t hash() const;// { return string ? string->hash : 0; }


	static void test();
//private:
	Reference<SharedImmutableStringData> string; // Null if this is the empty string, non-null otherwise.
};


[[nodiscard]] inline SharedImmutableString makeSharedImmutableString(const char* data, size_t len);
[[nodiscard]] inline SharedImmutableString makeSharedImmutableString(string_view str);
[[nodiscard]] inline SharedImmutableString makeEmptySharedImmutableString();



/*=====================================================================
SharedImmutableStringData
-------------------------

=====================================================================*/
class SharedImmutableStringData : public ThreadSafeRefCounted
{
public:
	inline SharedImmutableStringData(); // Initialise as an empty vector.
	inline ~SharedImmutableStringData();

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

	uint64 size_;
	uint64 hash;
};


[[nodiscard]] Reference<SharedImmutableStringData> makeSharedImmutableStringData(const char* data, size_t len);
[[nodiscard]] inline Reference<SharedImmutableStringData> makeSharedImmutableStringData(string_view str) { return makeSharedImmutableStringData(str.data(), str.size()); }
void doDestroySharedImmutableStringData(SharedImmutableStringData* str);



SharedImmutableString makeSharedImmutableString(const char* data, size_t len)
{ 
	return SharedImmutableString((len == 0) ? nullptr : makeSharedImmutableStringData(data, len));
}

SharedImmutableString makeSharedImmutableString(string_view str)
{ 
	return SharedImmutableString((str.size() == 0) ? nullptr : makeSharedImmutableStringData(str.data(), str.size()));
}

SharedImmutableString makeEmptySharedImmutableString() { return SharedImmutableString(nullptr); }




const size_t SharedImmutableString::size() const { return string ? string->size() : 0; }
const char* SharedImmutableString::data() const { return string ? string->data() : nullptr; }

bool SharedImmutableString::operator == (const SharedImmutableString& other) const
{
	if(string.isNull())
		return other.string.isNull();
		
	assert(string.nonNull());
	if(other.string.isNull())
		return false;

	return *string == *other.string;
}

bool SharedImmutableString::operator != (const SharedImmutableString& other) const { return !(*this == other); }

bool SharedImmutableString::operator < (const SharedImmutableString& other) const
{
	if(string.isNull() && other.string.nonNull())
		return true;
		
	if(string.nonNull() && other.string.isNull())
		return false;

	return string->compare(*other.string) < 0;
}

inline bool SharedImmutableString::pointerLessThan(const SharedImmutableString& other) const
{
	return string.ptr() < other.string.ptr();
}

bool SharedImmutableString::operator == (const std::string& other) const
{
	return toStringView() == other;
}

bool SharedImmutableString::operator != (const std::string& other) const
{
	return toStringView() != other;
}

string_view SharedImmutableString::toStringView() const { return string ? string_view(string->data(), string->size()) : string_view(); }
std::string SharedImmutableString::toString() const { return string ? std::string(string->data(), string->size()) : std::string(); }

size_t SharedImmutableString::hash() const { return string ? string->hash : 0; }


SharedImmutableStringData::SharedImmutableStringData()
:	size_(0), hash(0)
{}


SharedImmutableStringData::~SharedImmutableStringData()
{}


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


// For SharedImmutableStrings from a SharedStringTable, where pointer equality can be used.
struct SharedImmutableStringKeyPointerEqual
{
	bool operator() (const glare::SharedImmutableString& a, const glare::SharedImmutableString& b) const
	{
		return a.string.getPointer() == b.string.getPointer();
	}
};


struct SharedImmutableStringKeyValueEqual
{
	bool operator() (const glare::SharedImmutableString& a, const glare::SharedImmutableString& b) const
	{
		if(a.string.isNull() || b.string.isNull())
			return a.string.isNull() && b.string.isNull();

		return *a.string == *b.string;
	}
};


struct SharedImmutableStringHasher
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
