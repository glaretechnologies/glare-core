/*=====================================================================
GlareString.h
-------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "MemAlloc.h"
#include <cstring>
#include <assert.h>


/*=====================================================================
glare::String
-------------
String class with the small string optimisation.
Only works on little-endian systems currently.
=====================================================================*/
namespace glare
{

class String
{
public:
	inline String(); // Initialise as an empty vector.
	explicit inline String(size_t count, char val = 0); // Initialise with count copies of val.
	explicit inline String(const char* str);
	inline String(const String& other); // Initialise as a copy of other
	inline ~String();

	inline String& operator=(const String& other);

	static const uint32 ON_HEAP_BIT = 31u;
	static const uint32 ON_HEAP_BIT_MASK = 1u << ON_HEAP_BIT;

	// Size is stored in 31 bits, so max size is 2^31 - 1.
	static const size_t MAX_CAPACITY = (1u << 31) - 1;

	static const uint32 DIRECT_CAPACITY = 14;

	inline void reserve(size_t M); // Make sure capacity is at least M.
	
	//inline void resize(size_t new_size); // Resize to size N, using default constructor if N > size().
	inline void resize(size_t new_size, char val = 0); // Resize to size new_size, using copies of val if new_size > size().
	inline void resizeNoCopy(size_t new_size); // Resizes, but doesn't copy old data or initialise new data.  It does however null-terminate the string.
	// Only use if new data is going to be completely written to before reading from it.

	inline size_t capacity() const { return storingOnHeap() ? capacity_storage : DIRECT_CAPACITY; }
	inline size_t size() const { return storingOnHeap() ? (on_heap_and_size & ~ON_HEAP_BIT_MASK) : (on_heap_and_size >> 24u); }
	inline bool empty() const { return size() == 0; }

	inline char*       data()        { return storingOnHeap() ? e : (char*)&e; }
	inline const char* data()  const { return storingOnHeap() ? e : (char*)&e; }
	inline const char* c_str() const { return storingOnHeap() ? e : (char*)&e; }

	inline void push_back(char t);
	inline void pop_back();

	inline void operator += (const String& other);

	inline char& operator[](size_t index);
	inline char operator[](size_t index) const;

	inline bool operator == (const String& other) const;
	inline bool operator != (const String& other) const;

	inline bool operator == (const char* other) const;
	inline bool operator != (const char* other) const;

	typedef char* iterator;
	typedef const char* const_iterator;

	
	static void test();
	
private:
	char* allocOnHeapForSize(size_t n);  // Allocate new memory on heap, with room for null terminator. So actually allocates n + 1 bytes.
	// Also throws exception if n > MAX_CAPACITY

	inline void reserveNoCopy(size_t M); // Make sure capacity is at least M.  Doesn't copy old data.

	/*
	Stored on heap layout:
	|------------------------------------------------
	|          e           |  capacity_ |  on_h..   |
	-------------------------------------------------

	Stored directly layout:
	-------------------------------------------------
	|h  e  l  l  o  _  w  o  r  l  d  _  w  h  \0|  |
	-------------------------------------------------
	 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15

	note that byte 15, on little-endian systems, is the most significant byte of on_heap_and_size.


	byte 15 layout:
	--------------------------
	|   | size (7 bits)      |
	------------------------- 
	  ^                       
	on heap bit (bit 31)

	*/
	inline bool storingOnHeap() const { return (on_heap_and_size & ON_HEAP_BIT_MASK) != 0; } // BitUtils::isBitSet(on_heap_and_size, ON_HEAP_BIT_MASK); }

	inline void setHeapBitAndSizeForDirectStorage(size_t s)
	{
		assert(s <= DIRECT_CAPACITY);
		on_heap_and_size = (on_heap_and_size & 0x00FFFFFFu) // Clear most significant byte
			| ((uint32)s << 24);
	}

	inline void setHeapBitAndSizeForHeapStorage(size_t s)
	{
		assert(s <= 2147483648u);
		on_heap_and_size = (1u << 31) | (uint32)s;
	}

	char* e;
	uint32 capacity_storage;
	uint32 on_heap_and_size; // Number of elements in the vector.  Elements e[0] to e[size_-1] are proper constructed objects.
};


String::String()
:	on_heap_and_size(0), e(nullptr), capacity_storage(DIRECT_CAPACITY)
{}


String::String(size_t count, char val)
{
	if(count <= DIRECT_CAPACITY)
	{
		e = nullptr;
		capacity_storage = 0;
		on_heap_and_size = 0;

		// Store directly
		char* const direct = (char*)&e;
		for(size_t i=0; i<count; ++i)
			direct[i] = val;

		direct[count] = '\0'; // Null terminate string.

		setHeapBitAndSizeForDirectStorage(count);
	}
	else
	{
		// Use heap storage
		e = allocOnHeapForSize(count); // Allocate new memory on heap

		for(size_t i=0; i<count; ++i)
			e[i] = val;

		e[count] = '\0'; // Null terminate string.

		capacity_storage = (uint32)count;
		setHeapBitAndSizeForHeapStorage(count);
	}
}


String::String(const char* str)
{
	const size_t count = std::strlen(str);

	if(count <= DIRECT_CAPACITY)
	{
		// Store directly
		std::memcpy(&e, str, count + 1); // Copy null terminator as well.

		setHeapBitAndSizeForDirectStorage(count);
	}
	else
	{

		// Use heap storage
		e = allocOnHeapForSize(count); // Allocate new memory on heap.

		std::memcpy(e, str, count + 1); // Copy null terminator as well.

		capacity_storage = (uint32)count;
		setHeapBitAndSizeForHeapStorage(count);
	}
}


String::String(const String& other)
{
	if(!other.storingOnHeap())
	{
		e = other.e;
		capacity_storage = other.capacity_storage;
		on_heap_and_size = other.on_heap_and_size;
	}
	else
	{
		const uint32 use_size = (uint32)other.size();
		e = allocOnHeapForSize(use_size); // Allocate new memory on heap.
		capacity_storage = use_size;
		setHeapBitAndSizeForHeapStorage(use_size);

		std::memcpy(e, other.e, use_size + 1); // Copy null terminator as well.
	}
}


String::~String()
{
	if(storingOnHeap())
		MemAlloc::alignedFree(e);
}


String& String::operator=(const String& other)
{
	if(this == &other)
		return *this;

	if(storingOnHeap())
		MemAlloc::alignedFree(e);

	if(!other.storingOnHeap())
	{
		e = other.e;
		capacity_storage = other.capacity_storage;
		on_heap_and_size = other.on_heap_and_size;
	}
	else
	{
		const uint32 use_size = (uint32)other.size();
		e = allocOnHeapForSize(use_size); // Allocate new memory on heap, + 1 for null terminator
		capacity_storage = use_size;
		setHeapBitAndSizeForHeapStorage(use_size);

		std::memcpy(e, other.e, use_size + 1); // Copy null terminator as well.
	}

	return *this;
}


void String::reserve(size_t n)
{
	if(n > capacity()) // If need to expand capacity:
	{
		if(n > DIRECT_CAPACITY)
		{
			// Allocate new memory
			char* new_e = allocOnHeapForSize(n);

			const size_t cur_size = size();

			// Copy from existing data
			if(storingOnHeap()) // If we were storing on heap:
			{
				std::memcpy(new_e, e, cur_size + 1); // Copy including null terminator
		
				MemAlloc::alignedFree(e); // Free old buffer.
			}
			else // Else if we were storing directly (not on heap):
			{
				std::memcpy(new_e, (char*)&e, cur_size + 1);  // Copy data from direct storage to heap, including null terminator
			}

			e = new_e;
			capacity_storage = (uint32)n;
			setHeapBitAndSizeForHeapStorage(cur_size);
		}
	}
}


void String::reserveNoCopy(size_t n)
{
	if(n > capacity()) // If need to expand capacity:
	{
		if(n > DIRECT_CAPACITY)
		{
			// Allocate new memory
			char* new_e = allocOnHeapForSize(n);

			const size_t cur_size = size();

			if(storingOnHeap()) // If we were storing on heap:
				MemAlloc::alignedFree(e); // Free old buffer.

			e = new_e;
			capacity_storage = (uint32)n;
			setHeapBitAndSizeForHeapStorage(cur_size);
		}
	}
}


#if 0
void String::resize(size_t new_size)
{
	if(storingOnHeap())
	{
		if(new_size <= capacity_storage)
		{
			// We could free the heap data and store directly, but just keep on the heap for now.
		}
		else // else new_size > capacity_:
		{
			reserve(new_size);
			on_heap_and_size = ((uint32)new_size << 1) | ON_HEAP_BIT;
		}
	}
	else // Else if we are currently storing directly (not on heap):
	{
		if(new_size > DIRECT_CAPACITY) // If won't fit in direct storage:
		{
			// We need to start storing on the heap
			reserve(new_size);
			on_heap_and_size = ((uint32)new_size << 1) | ON_HEAP_BIT;
		}
		else
		{
			on_heap_and_size = on_heap_and_size & 0xFFFFFF00; // Zero out rightmost byte
			on_heap_and_size |= ((uint32)new_size << 1); // Store new size
		}
	}

	// null terminate
	(*this)[size()] = '\0'; // null terminate
}
#endif


void String::resize(size_t new_size, const char val)
{
	const size_t old_size = size();

	if(storingOnHeap())
	{
		if(new_size <= capacity_storage)
		{
			// We could free the heap data and store directly, but just keep on the heap for now.
			
			setHeapBitAndSizeForHeapStorage(new_size);
			
			e[new_size] = '\0'; // null terminate
		}
		else // else new_size > capacity_:
		{
			reserve(new_size);
			setHeapBitAndSizeForHeapStorage(new_size);

			// Initialise new data
			for(size_t i=old_size; i<new_size; ++i)
				e[i] = val;
			e[new_size] = '\0'; // null terminate
		}
	}
	else // Else if we are currently storing directly (not on heap):
	{
		if(new_size > DIRECT_CAPACITY) // If won't fit in direct storage:
		{
			// We need to start storing on the heap
			reserve(new_size);
			setHeapBitAndSizeForHeapStorage(new_size);

			// Initialise new data
			for(size_t i=old_size; i<new_size; ++i)
				e[i] = val;
			e[new_size] = '\0'; // null terminate
		}
		else
		{
			setHeapBitAndSizeForDirectStorage(new_size);

			// Initialise new data
			char* const direct = (char*)&e;
			for(size_t i=old_size; i<new_size; ++i)
				direct[i] = val;
			direct[new_size] = '\0'; // null terminate
		}
	}
}


void String::resizeNoCopy(size_t new_size)
{
	if(storingOnHeap())
	{
		if(new_size <= capacity_storage)
		{
			// We could free the heap data and store directly, but just keep on the heap for now.
			
			setHeapBitAndSizeForHeapStorage(new_size);
			
			e[new_size] = '\0'; // null terminate
		}
		else // else new_size > capacity_:
		{
			reserveNoCopy(new_size);
			setHeapBitAndSizeForHeapStorage(new_size);

			e[new_size] = '\0'; // null terminate
		}
	}
	else // Else if we are currently storing directly (not on heap):
	{
		if(new_size > DIRECT_CAPACITY) // If won't fit in direct storage:
		{
			// We need to start storing on the heap
			reserveNoCopy(new_size);
			setHeapBitAndSizeForHeapStorage(new_size);

			e[new_size] = '\0'; // null terminate
		}
		else
		{
			setHeapBitAndSizeForDirectStorage(new_size);

			// Initialise new data
			char* const direct = (char*)&e;
			direct[new_size] = '\0'; // null terminate
		}
	}
}


void String::push_back(const char t)
{
	const size_t old_size = size();
	const size_t old_capacity = capacity();
	const size_t new_size = old_size + 1;
	if(old_size == old_capacity)
	{
		const size_t double_cap = old_capacity;
		const size_t new_capacity = (new_size > double_cap) ? new_size : double_cap;
		reserve(new_capacity);
	}

	if(storingOnHeap())
	{
		e[old_size] = t;
		e[new_size] = '\0'; // null terminate

		setHeapBitAndSizeForHeapStorage(new_size);
	}
	else
	{
		char* const direct = (char*)&e;
		direct[old_size] = t;
		direct[new_size] = '\0'; // null terminate

		setHeapBitAndSizeForDirectStorage(new_size);
	}
}


void String::pop_back()
{
	assert(size() >= 1);

	const size_t new_size = size() - 1;
	if(storingOnHeap())
	{
		setHeapBitAndSizeForHeapStorage(new_size);

		e[new_size] = '\0'; // null terminate
	}
	else
	{
		setHeapBitAndSizeForDirectStorage(new_size);

		char* const direct = (char*)&e;
		direct[new_size] = '\0'; // null terminate
	}
}


void String::operator += (const String& other)
{
	const size_t old_size = size();
	const size_t other_size = other.size();
	reserve(old_size + other_size);

	const char* const src  = other.data();
	      char* const dest = data();
	for(size_t i=0; i<other_size; ++i)
		dest[old_size + i] = src[i];
	dest[old_size + other_size] = '\0'; // null terminate

	if(storingOnHeap())
		setHeapBitAndSizeForHeapStorage(old_size + other_size);
	else
		setHeapBitAndSizeForDirectStorage(old_size + other_size);
}


char& String::operator[](size_t index)
{
	assert(index < size());
	return storingOnHeap() ? e[index] : ((char*)&e)[index];
}


char String::operator[](size_t index) const
{
	assert(index < size());
	return storingOnHeap() ? e[index] : ((char*)&e)[index];
}


bool String::operator == (const String& other) const
{
	const size_t this_size = size();
	if(this_size != other.size())
		return false;
	const char* const this_data  = data();
	const char* const other_data = other.data();
	for(size_t i=0; i != this_size; ++i)
		if(this_data[i] != other_data[i])
			return false;
	return true;
}


bool String::operator != (const String& other) const
{
	return !(*this == other);
}


bool String::operator == (const char* other) const
{
	const size_t other_size = std::strlen(other);
	if(size() != other_size)
		return false;
	const char* const this_data = data();
	for(size_t i=0; i != other_size; ++i)
		if(this_data[i] != other[i])
			return false;
	return true;
}


bool String::operator != (const char* other) const
{
	return !(*this == other);
}

} // end namespace glare
