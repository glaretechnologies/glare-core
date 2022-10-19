/*=====================================================================
RequestInfo.cpp
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "RequestInfo.h"


#include <WebsiteExcep.h>
#include <StringUtils.h>


namespace web
{


RequestInfo::RequestInfo()
:	tls_connection(false),
	fuzzing(false)
{
}


RequestInfo::~RequestInfo()
{
}


UnsafeString RequestInfo::getPostField(const std::string& key) const
{
	for(size_t i=0; i<post_fields.size(); ++i)
		if(post_fields[i].key == key)
			return post_fields[i].value;
	return "";
}


int RequestInfo::getPostIntField(const std::string& key) const // Throws WebsiteExcep on failure
{
	try
	{
		UnsafeString s = getPostField(key);
		if(s.empty())
			throw WebsiteExcep("value for key '" + key + "' not found or empty");
		else
			return stringToInt(s.str());
	}
	catch(StringUtilsExcep& )
	{
		throw WebsiteExcep("value for key '" + key + "' could not be parsed as an int.");
	}
}


UnsafeString RequestInfo::getURLParam(const std::string& key) const
{
	for(size_t i=0; i<URL_params.size(); ++i)
		if(URL_params[i].key == key)
			return URL_params[i].value;
	return "";
}


int RequestInfo::getURLIntParam(const std::string& key) const // Throws WebsiteExcep on failure
{
	try
	{
		UnsafeString s = getURLParam(key);
		if(s.empty())
			throw WebsiteExcep("value for key '" + key + "' not found or empty");
		else
			return stringToInt(s.str());
	}
	catch(StringUtilsExcep& )
	{
		throw WebsiteExcep("value for key '" + key + "' could not be parsed as an int.");
	}
}


} // end namespace web
