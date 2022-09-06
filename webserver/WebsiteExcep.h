/*=====================================================================
WebsiteExcep.h
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include <Exception.h>


namespace web
{


/*=====================================================================
WebsiteExcep
------------

=====================================================================*/
class WebsiteExcep : public glare::Exception
{
public:
	WebsiteExcep(const std::string& text_) : glare::Exception(text_) {}
};


} // end namespace web
