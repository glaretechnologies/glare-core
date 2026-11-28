/*=====================================================================
StringBuilder.h
---------------
Copyright Glare Technologies Limited 2026 - 
=====================================================================*/
#pragma once


#include "SmallVector.h"
#include "StringUtils.h"
#include "string_view.h"


/*=====================================================================
StringBuilder
-------------
A class for building a string made of multiple sub-strings, while avoiding
memory allocations.
Uses a SmallVector with a 512 byte internal buffer to avoid an initial heap alloc.
=====================================================================*/
struct StringBuilder
{
	void append(const string_view s)
	{
		if(s.size() > 0)
		{
			const size_t write_i = buf.size();
			buf.resize(write_i + s.size());
			std::memcpy(buf.data() + write_i, s.data(), s.size());
		}
	}

	void appendInt(int x)
	{
		append(::toString(x)); // Just rely on small-string optimisation to avoid heap allocation here.
	}

	void appendSizeT(size_t x)
	{
		append(::toString(x)); // Just rely on small-string optimisation to avoid heap allocation here.
	}

	void removeSuffixInPlace(const string_view suffix)
	{
		if(::hasSuffix(string_view(buf.data(), buf.size()), suffix))
			buf.resize(buf.size() - suffix.size());
	}

	void clear()
	{
		buf.resize(0);
	}

	// NOTE: Do not append() the string view returned from this call to the same builder!  It may crash!
	string_view getStringView() const { return string_view(buf.data(), buf.size()); }

	static void test();

	SmallVector<char, 512> buf;
};
