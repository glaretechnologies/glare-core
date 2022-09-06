/*=====================================================================
UnsafeString.h
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include <string>
class OutStream;
class InStream;


namespace web
{


/*=====================================================================
UnsafeString
-------------------
A string whose contents have been set by a website user, and hence
must be escaped before being inserted into a page etc..
=====================================================================*/
class UnsafeString
{
public:
	UnsafeString();
	UnsafeString(const std::string& s_) : s(s_) {}
	UnsafeString(const char* s_) : s(s_) {}
	~UnsafeString();


	void writeToStream(OutStream& s) const;
	void readFromStream(InStream& s);

	const std::string& str() const { return s; }

	const std::string HTMLEscaped() const;
	const std::string URLEscaped() const;

	inline bool empty() const { return s.empty(); }

	inline bool operator == (const std::string& other) const { return s == other; }

private:
	std::string s;
};


} // end namespace web
