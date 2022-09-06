/*=====================================================================
Escaping.cpp
------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "Escaping.h"


#include "TestUtils.h"
#include <StringUtils.h>


namespace web
{


/*
See http://www.rfc-editor.org/rfc/rfc1738.txt

Section 2.2:

" The characters ";",
"/", "?", ":", "@", "=" and "&" are the characters which may be
reserved for special meaning within a scheme.
"

"Thus, only alphanumerics, the special characters "$-_.+!*'(),", and
reserved characters used for their reserved purposes may be used
unencoded within a URL."

Since we're encoding ' ' to '+', we'll encode '+' as well, to guarantee a one-to-one mapping
*/
const std::string Escaping::URLEscape(const std::string& s)
{
	std::string result;
	result.reserve(s.size());

	for(size_t i=0; i<s.size(); ++i)
	{
		if(::isAlphaNumeric(s[i]) || 
			s[i] == '$' ||
			s[i] == '-' ||
			s[i] == '_' ||
			s[i] == '.' ||
			s[i] == '!' ||
			s[i] == '*' ||
			s[i] == '\'' ||
			s[i] == '(' ||
			s[i] == ')' ||
			s[i] == ',')
		{
			result.push_back(s[i]);
		}
		else if(s[i] == ' ')
		{
			result.push_back('+');
		}
		else
		{
			// Encode using '%'.
			// TODO: How is unicode handled?  Is unicode valid in URLs?
			result.push_back('%');
			result += ::toHexString((uint64)s[i]);
		}
	}
	return result;
}


const std::string Escaping::URLUnescape(const std::string& s)
{
	std::string result;
	result.reserve(s.size());

	int outpos = 0;

	for(size_t i=0; i<s.size(); ++i)
	{
		if(s[i] == '+')
		{
			result.resize(result.size() + 1);
			result[outpos] = ' ';
		}
		else if(s[i] == '%' && ((i + 2) < s.size()))
		{
			unsigned int nibble_a = hexCharToUInt(s[i+1]);
			unsigned int nibble_b = hexCharToUInt(s[i+2]);
			result.resize(result.size() + 1);
			// TODO: handle char out of range
			result[outpos] = (char)((nibble_a << 4) + nibble_b);
			i += 2;
		}
		else
		{
			result.resize(result.size() + 1);
			result[outpos] = s[i];
		}
		outpos++;
	}
	return result;
}


/*
Replace "<" with "&lt;" etc..

*/
const std::string Escaping::HTMLEscape(const std::string& s)
{
	std::string result;
	result.reserve(s.size());

	for(size_t i=0; i<s.size(); ++i)
	{
		if(s[i] == '<')
			result += "&lt;";
		else if(s[i] == '>')
			result += "&gt;";
		else if(s[i] == '"')
			result += "&quot;";
		else if(s[i] == '&')
			result += "&amp;";
		else
			result.push_back(s[i]);
	}
	return result;
}


/*
Replace "&lt;" with "<" etc..

*/
const std::string Escaping::HTMLUnescape(const std::string& s)
{
	std::string result;
	result.reserve(s.size());

	const size_t s_size = s.size();

	for(size_t i=0; i<s.size(); )
	{
		if(s[i] == '&' && (i + 1 < s_size))
		{
			if(s[i+1] == 'l' && (i + 3 < s_size) && s[i+2] == 't' && s[i+3] == ';') // "&lt;"
			{
				const size_t result_size = result.size();
				result.resize(result_size + 1);
				result[result_size] = '<';
				i += 4;
			}
			else if(s[i+1] == 'g' && (i + 3 < s_size) && s[i+2] == 't' && s[i+3] == ';') // "&gt;"
			{
				const size_t result_size = result.size();
				result.resize(result_size + 1);
				result[result_size] = '>';
				i += 4;
			}
			else if(s[i+1] == 'q' && (i + 5 < s_size) && s[i+2] == 'u' && s[i+3] == 'o' && s[i+4] == 't' && s[i+5] == ';') // "&quot;"
			{
				const size_t result_size = result.size();
				result.resize(result_size + 1);
				result[result_size] = '"';
				i += 6;
			}
			else if(s[i+1] == 'a' && (i + 4 < s_size) && s[i+2] == 'm' && s[i+3] == 'p' && s[i+4] == ';') // "&amp;"
			{
				const size_t result_size = result.size();
				result.resize(result_size + 1);
				result[result_size] = '&';
				i += 5;
			}
			else
			{
				// Didn't match any special entity
				const size_t result_size = result.size();
				result.resize(result.size() + 1);
				result[result_size] = s[i];
				i++;
			}
		}
		else
		{
			const size_t result_size = result.size();
			result.resize(result.size() + 1);
			result[result_size] = s[i];
			i++;
		}
	}
	return result;
}


/*
JSON RFC:
http://tools.ietf.org/html/rfc7159

"All Unicode characters may be placed within the
quotation marks, except for the characters that must be escaped:
quotation mark, reverse solidus, and the control characters (U+0000
through U+001F)."
*/
const std::string Escaping::JSONEscape(const std::string& s)
{
	std::string result;
	result.reserve(s.size());

	for(size_t i=0; i<s.size(); ++i)
	{
		if(s[i] == '"')
			result += "\\\"";
		else if(s[i] == '\\') // a single backslash needs to be escaped as two backslashes.
			result += "\\\\";
		else if(s[i] >= 0 && s[i] <= 0x1F) // control characters (U+0000 through U+001F).
			result += "\\u" + leftPad(toString(s[i]), '0', 4);
		else
			result.push_back(s[i]);
	}
	return result;
}


void Escaping::test()
{
	// Test URLEscape
	testAssert(Escaping::URLEscape("") == "");
	testAssert(Escaping::URLEscape("a") == "a");
	testAssert(Escaping::URLEscape("#") == "%23");
	testAssert(Escaping::URLEscape("a#") == "a%23");
	testAssert(Escaping::URLEscape("#a") == "%23a");
	testAssert(Escaping::URLEscape("##") == "%23%23");

	// Test URLEscape of space
	testAssert(Escaping::URLEscape(" ") == "+");

	// Test URLEscape of plus
	testAssert(Escaping::URLEscape("+") == "%2B");

	testAssert(Escaping::URLEscape("+ +") == "%2B+%2B");

	// Test some characters that should not be escaped
	testAssert(Escaping::URLEscape("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") == "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
	testAssert(Escaping::URLEscape("0123456789") == "0123456789");
	testAssert(Escaping::URLEscape("$-_.!*'(),") == "$-_.!*'(),");






	// Test HTMLEscape

	testAssert(Escaping::HTMLEscape("") == "");
	testAssert(Escaping::HTMLEscape("a") == "a");
	testAssert(Escaping::HTMLEscape("<") == "&lt;");
	testAssert(Escaping::HTMLEscape("<a") == "&lt;a");
	testAssert(Escaping::HTMLEscape("a<") == "a&lt;");
	testAssert(Escaping::HTMLEscape("<<") == "&lt;&lt;");

	testAssert(Escaping::HTMLEscape("< > \" &") == "&lt; &gt; &quot; &amp;");

	// Test HTMLUnescape
	testAssert(Escaping::HTMLUnescape("") == "");
	testAssert(Escaping::HTMLUnescape("a") == "a");
	testAssert(Escaping::HTMLUnescape("<") == "<");
	
	testAssert(Escaping::HTMLUnescape("&") == "&");
	testAssert(Escaping::HTMLUnescape("&l") == "&l");
	testAssert(Escaping::HTMLUnescape("&lt") == "&lt");
	testAssert(Escaping::HTMLUnescape("&lt;") == "<");
	testAssert(Escaping::HTMLUnescape("&lt;A") == "<A");

	testAssert(Escaping::HTMLUnescape("&") == "&");
	testAssert(Escaping::HTMLUnescape("&g") == "&g");
	testAssert(Escaping::HTMLUnescape("&gt") == "&gt");
	testAssert(Escaping::HTMLUnescape("&gt;") == ">");

	testAssert(Escaping::HTMLUnescape("&") == "&");
	testAssert(Escaping::HTMLUnescape("&q") == "&q");
	testAssert(Escaping::HTMLUnescape("&qu") == "&qu");
	testAssert(Escaping::HTMLUnescape("&quo") == "&quo");
	testAssert(Escaping::HTMLUnescape("&quot") == "&quot");
	testAssert(Escaping::HTMLUnescape("&quot;") == "\"");

	testAssert(Escaping::HTMLUnescape("&lt; &gt; &quot; &amp;") == "< > \" &");

}


} // end namespace web
