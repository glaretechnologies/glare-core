/*=====================================================================
UnsafeString.cpp
----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "UnsafeString.h"


#include <InStream.h>
#include <OutStream.h>
#include "Escaping.h"
#include "WebsiteExcep.h"


namespace web
{


UnsafeString::UnsafeString()
{

}


UnsafeString::~UnsafeString()
{

}


void UnsafeString::writeToStream(OutStream& stream) const
{
	stream.writeStringLengthFirst(s);
}


void UnsafeString::readFromStream(InStream& stream)
{
	s = stream.readStringLengthFirst(/*max string length=*/100000000);
}


const std::string UnsafeString::HTMLEscaped() const
{
	return Escaping::HTMLEscape(s);
}


const std::string UnsafeString::URLEscaped() const
{
	return Escaping::URLEscape(s);
}


} // end namespace web
