/*=====================================================================
Exception.h
-----------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
// Not using pragma once since we copy this file and pragma once is path based
#ifndef GLARE_EXCEPTION_H
#define GLARE_EXCEPTION_H


#include <string>


// Base exception class
namespace Indigo
{


class Exception
{
public:
	Exception(const std::string& s_) : s(s_) {}
	~Exception(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};


}


#endif // GLARE_EXCEPTION_H
