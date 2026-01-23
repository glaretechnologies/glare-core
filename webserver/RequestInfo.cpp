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
	fuzzing(false),
	deflate_accept_encoding(false),
	zstd_accept_encoding(false)
{
}


RequestInfo::~RequestInfo()
{
}


UnsafeString RequestInfo::getPostField(const std::string& key) const
{
	for(size_t i=0; i<post_fields.size(); ++i)
		if(post_fields[i]->key == key)
			return UnsafeString(std::string((const char*)post_fields[i]->content.data(), post_fields[i]->content.size()));
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


double RequestInfo::getPostDoubleField(const std::string& key) const
{
	try
	{
		UnsafeString s = getPostField(key);
		if(s.empty())
			throw WebsiteExcep("value for key '" + key + "' not found or empty");
		else
			return stringToDouble(s.str());
	}
	catch(StringUtilsExcep& )
	{
		throw WebsiteExcep("value for key '" + key + "' could not be parsed as an int.");
	}
}


bool RequestInfo::isURLParamPresent(const std::string& key) const
{
	for(size_t i=0; i<URL_params.size(); ++i)
		if(URL_params[i].key == key)
			return true;
	return false;
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


double RequestInfo::getURLDoubleParam(const std::string& key) const
{
	try
	{
		UnsafeString s = getURLParam(key);
		if(s.empty())
			throw WebsiteExcep("value for key '" + key + "' not found or empty");
		else
			return stringToDouble(s.str());
	}
	catch(StringUtilsExcep& )
	{
		throw WebsiteExcep("value for key '" + key + "' could not be parsed as a double.");
	}
}


Reference<FormField> RequestInfo::getPostFieldForName(const std::string& name) const
{
	for(size_t i=0; i<post_fields.size(); ++i)
		if(post_fields[i]->key == name)
			return post_fields[i];

	throw WebsiteExcep("Could not find post field with name '" + name + "'");
}


Reference<FormField> RequestInfo::getPostFieldForNameIfPresent(const std::string& name) const
{
	for(size_t i=0; i<post_fields.size(); ++i)
		if(post_fields[i]->key == name)
			return post_fields[i];

	throw Reference<FormField>();
}


std::string RequestInfo::getHostHeader() const
{
	for(size_t i=0; i<headers.size(); ++i)
		if(StringUtils::equalCaseInsensitive(headers[i].key, "host"))
			return toString(headers[i].value);

	return std::string();
}


} // end namespace web
