/*=====================================================================
Escaping.h
----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "UnsafeString.h"
#include <string>


/*=====================================================================
Escaping
-------------------

=====================================================================*/
namespace web
{


class Escaping
{
public:

	static const std::string URLEscape(const std::string& s);
	//inline static const std::string URLEscape(const UnsafeString& s) { return URLEscape(s.str()); }


	static const std::string URLUnescape(const std::string& s);

	/*
	Replace "<" with "&lt;" etc..

	*/
	static const std::string HTMLEscape(const std::string& s);

	/*
	Replace "&lt;" with "<" etc..

	*/
	static const std::string HTMLUnescape(const std::string& s);


	static const std::string JSONEscape(const std::string& s);

	static const std::string JavascriptEscape(const std::string& s) { return JSONEscape(s); }


	static void test();
};


} // end namespace web
