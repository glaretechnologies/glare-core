/*=====================================================================
IndigoException.h
-----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "IndigoString.h"


namespace Indigo
{


///
/// Exception class.
/// Contains a string member that describes the exception.
///
class IndigoException
{
public:
	USE_INDIGO_SDK_ALLOCATOR

	IndigoException(const String& s_) : s(s_) {}
	~IndigoException() {}

	///
	/// Returns a description of the exception.
	///
	const String& what() const { return s; }

private:
	String s;
};


}
