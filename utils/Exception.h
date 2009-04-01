/*=====================================================================
Exception.h
-----------
File created by ClassTemplate on Wed Apr 01 11:06:48 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __EXCEPTION_H_666_
#define __EXCEPTION_H_666_


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


#endif //__EXCEPTION_H_666_
