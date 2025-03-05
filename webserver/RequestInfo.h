/*=====================================================================
RequestInfo.h
-------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include <networking/IPAddress.h>
#include <UnsafeString.h>
#include <Reference.h>
#include <string_view.h>
#include <string>
#include <vector>
class OutStream;


namespace web
{


class URLParam
{
public:
	std::string key;
	UnsafeString value;
};

class FormField
{
public:
	std::string key;
	UnsafeString value;
};


class Cookie
{
public:
	std::string key;
	std::string value;
};

class Header
{
public:
	string_view key; // NOTE: these are unsafe strings also.
	string_view value;
};

class Range
{
public:
	Range(){}
	Range(int64 s, int64 e) : start(s), end_incl(e) {}
	int64 start;
	int64 end_incl; // -1 if not set present, e.g. for cases like "<unit>=<range-start>- "  NOTE: this is an inclusive end value, so this is the index of the last byte in the range.
	bool operator == (const Range& other) const { return start == other.start && end_incl == other.end_incl; }
};


/*=====================================================================
RequestInfo
-------------------

=====================================================================*/
class RequestInfo
{
public:
	RequestInfo();
	~RequestInfo();

	UnsafeString getPostField(const std::string& key) const; // Returns empty string if key not present.
	int getPostIntField(const std::string& key) const; // Throws WebsiteExcep on failure

	bool isURLParamPresent(const std::string& key) const;
	UnsafeString getURLParam(const std::string& key) const; // Returns empty string if key not present.
	int getURLIntParam(const std::string& key) const; // Throws WebsiteExcep on failure

	std::string getHostHeader() const; // Returns host header, or empty string if not present.

	std::string verb;
	std::string path;

	std::vector<Header> headers;
	std::vector<URLParam> URL_params;
	std::vector<FormField> post_fields;
	std::vector<Cookie> cookies;

	std::vector<uint8> post_content; // For POST request type.

	std::vector<Range> ranges;

	// Accept-encodings specified in header: (just the ones we are interested in)
	bool deflate_accept_encoding;
	bool zstd_accept_encoding;

	IPAddress client_ip_address;
	bool tls_connection;

	bool fuzzing;
};


class ReplyInfo
{
public:
	OutStream* socket;
};


} // end namespace web
