/*=====================================================================
jsstring.cpp
------------
File created by ClassTemplate on Mon Oct 25 01:56:30 2004Code By Nicholas Chapman.
=====================================================================*/
#include "jsstring.h"


#include <string.h>
#include <assert.h>

JSString::JSString()
{
	//data = NULL;
	data = new char[1];
	*data = '\0';
}

JSString::JSString(const char* str)
{
	//------------------------------------------------------------------------
	//alloc new space
	//------------------------------------------------------------------------
	data = new char[::strlen(str) + 1];

	//------------------------------------------------------------------------
	//copy incoming data to new space
	//------------------------------------------------------------------------
	::strcpy(data, str);
}

JSString::JSString(const JSString& other)
:	data(NULL)
{
	*this = other;
}

JSString::~JSString()
{
	delete data;
}



JSString& JSString::operator = (const JSString& other)
{
	if(this == &other)
		return *this;

	//------------------------------------------------------------------------
	//release old data
	//------------------------------------------------------------------------
	delete[] data;

	//------------------------------------------------------------------------
	//alloc new space
	//------------------------------------------------------------------------
	data = new char[other.size() + 1];

	//------------------------------------------------------------------------
	//copy incoming data to new space
	//------------------------------------------------------------------------
	::strcpy(data, other.c_str());

	return *this;
}

bool JSString::operator == (const JSString& other) const
{
	if(size() != other.size())
		return false;

	const int sz = size();

	for(int i=0; i<sz; ++i)
	{
		if((*this)[i] != other[i])
			return false;
	}

	return true;
}

bool JSString::operator != (const JSString& other) const
{
	/*if(size() != other.size())
		return false;

	const int sz = size();

	for(int i=0; i<sz; ++i)
	{
		if((*this)[i] != other[i])
			return false;
	}

	return true;*/

	//NOTE: inefficient
	return !((*this) == other);
}

bool JSString::operator == (const char* otherstr) const
{
	assert(data);
	assert(otherstr);

	return ::strcmp(data, otherstr) == 0;
}

bool JSString::operator != (const char* otherstr) const
{
	assert(data);
	assert(otherstr);

	return ::strcmp(data, otherstr) != 0;
}

//not counting NULL terminator.
int JSString::size() const
{
	return ::strlen(data);
}


	
void JSString::doUnitTests()
{
	JSString s1("hello");

	assert(s1 == "hello");
	assert(s1 == JSString("hello"));

	JSString s2("");

	assert(s2 == "");
	assert(s2 == JSString(""));
}
