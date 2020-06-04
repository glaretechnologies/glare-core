/*=====================================================================
Exception.h
-----------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
// Not using pragma once since we copy this file and pragma once is path based
#ifndef GLARE_EXCEPTION_H
#define GLARE_EXCEPTION_H


#include <string>


namespace Indigo
{


// Base exception class
class Exception
{
public:
	Exception(const std::string& s_) : s(s_) {}
	~Exception(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};


// An exception that is thrown when an operation is cancelled, e.g. when ShouldCancelCallback::shouldCancel() returns true.
// This is split out into another class so exception-handling code can handle cancelling differently from other exceptions.
class CancelledException : public Exception
{
public:
	CancelledException();
};


}


#endif // GLARE_EXCEPTION_H
