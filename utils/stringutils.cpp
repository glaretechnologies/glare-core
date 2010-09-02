// Copyright Glare Technologies Limited 2009 -
#include "stringutils.h"

#include <cmath>
#include <stdarg.h>//NOTE: fixme
#include <stdlib.h>
#include <stdio.h>
#include <clocale>
#include "../utils/timer.h"
#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include <limits>


#if defined(WIN32) || defined(WIN64)
// Stop windows.h from defining the min() and max() macros
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif


float stringToFloat(const std::string& s) // throws StringUtilsExcep
{
	char* end_ptr = NULL;
	const float ret = (float)strtod(s.c_str(), &end_ptr);
	if(end_ptr == s.c_str())
		throw StringUtilsExcep("Failed to convert '" + s + "' to a float.");
	return ret;
}


double stringToDouble(const std::string& s) // throws StringUtilsExcep
{
	char* end_ptr = NULL;
	const double ret = strtod(s.c_str(), &end_ptr);
	if(end_ptr == s.c_str())
		throw StringUtilsExcep("Failed to convert '" + s + "' to a double.");
	return ret;
}


int stringToInt(const std::string& s) // throws StringUtilsExcep
{
	char* end_ptr = NULL;
	const int ret = strtol(s.c_str(), &end_ptr,
		10 // base
		);
	if(end_ptr == s.c_str())
		throw StringUtilsExcep("Failed to convert '" + s + "' to an int.");
	return ret;
}


uint64 stringToUInt64(const std::string& s) // throws StringUtilsExcep
{
	uint64 x = 0;
	uint64 valmul = 1;
	bool valmul_has_overflowed = false;

	//const uint64 max_uint64 = std::numeric_limits<uint64>::max(); //18446744073709551615LL

	for(int i=(int)s.size() - 1; i >= 0; --i)
	{
		if(s[i] >= '0' && s[i] <= '9')
		{
			const uint64 incr = valmul * (s[i] - '0');
			
			if(incr > std::numeric_limits<uint64>::max() - x) // If x will overflow when we add incr to it...
				throw StringUtilsExcep("Failed to convert '" + s + "' to an uint64: Value is too large.");
			if(valmul_has_overflowed && incr > 0)
				throw StringUtilsExcep("Failed to convert '" + s + "' to an uint64: Value is too large.");
				
			x += incr;
		}
		else
			throw StringUtilsExcep("Failed to convert '" + s + "' to an uint64: Invalid character '" + s[i] + "'");

		if(valmul > (std::numeric_limits<uint64>::max() / 10)) // if valmul will overflow when we multiply it by 10...
			valmul_has_overflowed = true; //throw StringUtilsExcep("Failed to convert '" + s + "' to an uint64: Value is too large.");

		valmul *= 10;
	}
	return x;
}


unsigned int hexCharToUInt(char c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	else if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else if(c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else
		return 0;
}


unsigned int hexStringToUInt(const std::string& s)
{
	if(s.size() < 3)
		return 0;//too short, parse error
	else if(s.size() > 10)
		return 0;//too long, parse error

	unsigned int i = 0;
	unsigned int x = 0;
	unsigned int nibble;

	//eat '0'
	if(s[i++] != '0')
		return 0;//parse error

	//eat 'x'
	if(s[i++] != 'x')
		return 0;//parse error

	while(i < s.size())
	{
		if(s[i] >= '0' && s[i] <= '9')
			nibble = s[i] - '0';
		else if(s[i] >= 'a' && s[i] <= 'f')
			nibble = s[i] - 'a' + 10;
		else if(s[i] >= 'A' && s[i] <= 'F')
			nibble = s[i] - 'A' + 10;
		else
			return 0;//parse error

		x <<= 4;
		x |= nibble;//set lower 4 bits to nibble

		i++;
	}

	return x;
}


unsigned long long hexStringTo64UInt(const std::string& s)
{
	assert(sizeof(unsigned long long) == 8);

	if(s.size() < 3)
		return 0;//too short, parse error
	else if(s.size() > 18)
		return 0;//too long, parse error

	unsigned int i = 0;
	unsigned int x = 0;
	unsigned int nibble;

	//eat '0'
	if(s[i++] != '0')
		return 0;//parse error

	//eat 'x'
	if(s[i++] != 'x')
		return 0;//parse error

	while(i < s.size())
	{
		if(s[i] >= '0' && s[i] <= '9')
			nibble = s[i] - '0';
		else if(s[i] >= 'a' && s[i] <= 'f')
			nibble = s[i] - 'a' + 10;
		else if(s[i] >= 'A' && s[i] <= 'F')
			nibble = s[i] - 'A' + 10;
		else
			return 0;//parse error

		x <<= 4;
		x |= nibble;//set lower 4 bits to nibble

		i++;
	}

	return x;
}


/*const std::string toHexString(unsigned int i)
{

	//------------------------------------------------------------------------
	//build the hex string in reverse order
	//------------------------------------------------------------------------
	std::string reverse_s;
	unsigned int nibble;

	while(i != 0)
	{
		nibble = i & 0x0000000F;//get last 4 bits
		if(nibble <= 9)
			concatWithChar(reverse_s, '0' + nibble - 0);
		else
			concatWithChar(reverse_s, 'a' + nibble - 10);

		i >>= 4;//shift right 4 bits
	}

	if(reverse_s.empty())
	{
		//hex constants must have at least one digit :)
		return "0x0";
	}
	else
	{
		std::string s;
		s.resize(reverse_s.size());
		for(unsigned int z=0; z<s.size(); ++z)
			s[z] = reverse_s[reverse_s.size() - z - 1];

		return "0x" + s;
	}
}*/


//for 64 bit integers
//NOTE: this function is unchanged from the 32 bit version... so turn into template?
//or could cast 32 bit ints into 64 bit ints and always use this version.
const std::string toHexString(unsigned long long i)
{
	assert(sizeof(unsigned long long) == 8);

	//------------------------------------------------------------------------
	//build the hex string in reverse order
	//------------------------------------------------------------------------
	std::string reverse_s;

	while(i != 0)
	{
		const unsigned long long nibble = i & 0x000000000000000F; // Get last 4 bits

		if(nibble <= 9)
			concatWithChar(reverse_s, '0' + (char)nibble - 0);
		else
			concatWithChar(reverse_s, 'A' + (char)nibble - 10);

		i >>= 4; // Shift right 4 bits
	}

	if(reverse_s.empty())
	{
		// Hex constants must have at least one digit :)
		return "0";
	}
	else
	{
		std::string s;
		s.resize(reverse_s.size());
		for(unsigned int z=0; z<s.size(); ++z)
			s[z] = reverse_s[reverse_s.size() - z - 1];

		return s;
	}
}


const std::string intToString(int i)
{
	// Not static for thread-safety.
	char buffer[16];

	sprintf(buffer, "%i", i);

	return std::string(buffer);
}


const std::string floatToString(float f)
{
	// Not static for thread-safety.
	char buffer[100];

	sprintf(buffer, "%f", f);

	return std::string(buffer);
}


const std::string doubleToString(double d, int num_decimal_places)
{
	assert(num_decimal_places >= 0 && num_decimal_places <= 100);

	//not static for thread-safety.
	char buffer[512];//double can be up to ~1x10^38 :)

	sprintf(buffer, std::string("%1." + toString(num_decimal_places) + "f").c_str(), d);
	//sprintf(buffer, std::string("%1." + toString(num_decimal_places) + "E").c_str(), d);

	return std::string(buffer);
}


const std::string doubleToStringScientific(double d, int num_decimal_places)
{
	assert(num_decimal_places >= 0 && num_decimal_places <= 100);
	char buffer[512];//double can be up to ~1x10^38 :)
	sprintf(buffer, std::string("%1." + toString(num_decimal_places) + "E").c_str(), d);
	return std::string(buffer);
}



const std::string floatToString(float f, int num_decimal_places)
{
	//not static for thread-safety.
	char buffer[100];


	assert(num_decimal_places >= 0);

	if(num_decimal_places >= 10)
		num_decimal_places = 9;

	const std::string format_string = "%1." + ::toString(num_decimal_places) + "f";

	sprintf(buffer, format_string.c_str(), f);

	return std::string(buffer);


	//not static for thread-safety.
	/*char buffer[100];

	const std::string format_string = "%1.xf";

	assert(num_decimal_places >= 0);

	if(num_decimal_places >= 10)
		num_decimal_places = 9;

	const std::string dec_string = intToString(num_decimal_places);
	assert(dec_string.size() == 1);

	format_string[3] = dec_string[0];
	//format[4] = 'f';
	//format[5] = 0;

	//"%1.2f"

	sprintf(buffer, format_string.c_str(), f);

	return std::string(buffer);*/
}


const std::string toString(unsigned int x)
{
	char buffer[16];

	sprintf(buffer, "%u", x);

	return std::string(buffer);
}


// Slooooow
const std::string toStringStrStream(unsigned int x)
{
	std::ostringstream stream;
	stream << x;
	return stream.str();
}


const std::string toString(uint64 x)
{
	char buffer[32];

	// u = unsigned decimal
#if (defined(WIN32) || defined(WIN64)) && !defined(__MINGW32__)
	// See http://msdn.microsoft.com/en-us/library/tcxf1dw6%28VS.80%29.aspx
	sprintf(buffer, "%I64u", x);
#else
	sprintf(buffer, "%llu", x);
#endif

	return std::string(buffer);
}


const std::string toString(int64 x)
{
	char buffer[32];

	// i = signed decimal
#if (defined(WIN32) || defined(WIN64)) && !defined(__MINGW32__)
	// See http://msdn.microsoft.com/en-us/library/tcxf1dw6%28VS.80%29.aspx
	sprintf(buffer, "%I64i", x);
#else
	sprintf(buffer, "%lli", x);
#endif

	return std::string(buffer);
}


const std::string boolToString(bool b)
{
	return b ? "true" : "false";
}


const std::string getLineFromText(int linenum, const char* textbuffer, int textbufferlength)
{
	assert(linenum >= 1);
	assert(textbuffer);
	assert(textbufferlength >= 0);

	//-----------------------------------------------------------------
	//go through text to 1st char after 'linenum' - 1 newlines
	//-----------------------------------------------------------------
	int curline = 1;
	int pos = 0;
	if(linenum > 1)
	{
		for(int i=0; i<textbufferlength; i++)
		{
			const char currentchar = textbuffer[i];
			if(currentchar == '\n')
			{
				curline++;

				if(curline == linenum)
				{
					pos = i;//save position of the '\n'
					break;
				}
			}
		}
	}


	pos++;//move to char after the '\n'

	if(curline < linenum)
		return "---no such line---";

	std::string linestring;

	for(int i=pos; i<textbufferlength; i++)
	{
		const char currentchar = textbuffer[i];

		if(currentchar == '\n')
			break;

		linestring += textbuffer[i];
	}

	assert(linestring.size() >= 1);

	//linestring.resize( linestring.size() - 1);
		//just lop off the last char

	//NOTE: lop off the cariage return here?

	return linestring;
}


const std::string eatHeadWhitespace(const std::string& text)
{
	for(unsigned int i=0; i<text.size(); i++)
	{
		if(!isWhitespace(text[i]))
		{
			//non-whitespace starts here

		//	basic_string substr(size_type pos = 0,size_type n = npos) const;
			return text.substr(i, text.size() - i);
		}
	}

	return text;
}


const std::string eatTailWhitespace(const std::string& text)
{
	for(int i=(int)text.size() - 1; i>=0; --i)//work backwards thru chars
	{
		if(!isWhitespace(text[i]))
		{
			//non-whitespace ends here

			return text.substr(0, i+1);
		}
	}

	//if got here, everything was whitespace
	return "";
}


const std::string eatWhitespace(const std::string& s)
{
	std::string out;
	for(size_t i=0; i<s.size(); ++i)
	{
		if(!::isWhitespace(s[i]))
			out += std::string(1, s[i]);
	}
	return out;
}


const std::string eatPrefix(const std::string& s, const std::string& prefix)
{
	if(::hasPrefix(s, prefix))
		return s.substr(prefix.length(), (int)s.length() - (int)prefix.length());
	else
		return s;
}


const std::string eatSuffix(const std::string& s, const std::string& suffix)
{
	if(::hasSuffix(s, suffix))
		return s.substr(0, (int)s.length() - (int)suffix.length());
	else
		return s;
}


const std::string toLowerCase(const std::string& text)
{
	/*char* strbuffer = new char[text.length() + 1];

	strcpy(strbuffer, text.c_str());

	_strlwr(strbuffer);

	std::string lowercasestring(strbuffer);

	delete[] strbuffer;

	return lowercasestring;*/

	std::string lowerstr = text;
	for(unsigned int i=0; i<lowerstr.size(); ++i)
		if(lowerstr[i] >= 'A' && lowerstr[i] <= 'Z')
			lowerstr[i] = lowerstr[i] - 'A' + 'a';

	return lowerstr;
}


const std::string toUpperCase(const std::string& text)
{
	/*char* strbuffer = new char[text.length() + 1];

	strcpy(strbuffer, text.c_str());

	_strupr(strbuffer);

	std::string uppercasestring(strbuffer);

	delete[] strbuffer;

	return uppercasestring;*/
	std::string upperstr = text;
	for(unsigned int i=0; i<upperstr.size(); ++i)
		if(upperstr[i] >= 'a' && upperstr[i] <= 'z')
			upperstr[i] = upperstr[i] - 'a' + 'A';

	return upperstr;
}


char toLowerCase(char c)
{
	//TEMP NASTY HACK
	/*char hackstring[2] = { c, 0 };

	_strlwr(hackstring);

	return hackstring[0];*/

	if(c >= 'A' && c <= 'Z')
		return c - 'A' + 'a';
	else
		return c;
}


char toUpperCase(char c)
{
	//TEMP NASTY HACK
	/*char hackstring[2] = { c, 0 };

	_strupr(hackstring);

	return hackstring[0];*/

	if(c >= 'a' && c <= 'z')
		return c - 'a' + 'A';
	else
		return c;
}


bool hasExtension(const std::string& file, const std::string& extension)
{
	const std::string dot_plus_extension = "." + toUpperCase(extension);

	if(file.length() < extension.length() + 1)
		return false;

	if(toUpperCase(file.substr(file.length() - dot_plus_extension.length(), dot_plus_extension.length()))
			== dot_plus_extension)
		return true;
	else
		return false;
}


const std::string getExtension(const std::string& filename)//3 letter extension
{
	//if(filename.size() < 4)
	//	return filename;//error

	//return filename.substr(filename.size() - 3, 3);

	const std::string::size_type dot_index = filename.find_last_of(".");

	if(dot_index == std::string::npos)
		return "";
	else
		return getTailSubString(filename, dot_index + 1);
}


const std::string eatExtension(const std::string& filename)
{
	return eatSuffix(filename, getExtension(filename));
}


const std::string eatDotAndExtension(const std::string& filename)
{
	return ::eatSuffix(eatExtension(filename), ".");
}


bool hasPrefix(const std::string& s, const std::string& prefix)
{
	if(prefix.length() > s.length())
		return false;

	for(unsigned int i=0; i<prefix.length(); i++)
		if(prefix[i] != s[i])
			return false;

	return true;
}


bool hasSuffix(const std::string& s, const std::string& postfix)
{
	if(postfix.length() > s.length())
		return false;

	assert(s.length() >= postfix.length());

	const unsigned int s_offset = s.length() - postfix.length();

	for(unsigned int i=0; i<postfix.length(); ++i)
		if(s[s_offset + i] != postfix[i])
			return false;

	return true;
}


int getNumMatches(const std::string& s, char target)
{
	int num_matches = 0;

	for(unsigned int i=0; i<s.length(); i++)
		if(s[i] == target)
			num_matches++;

	return num_matches;
}


void tokenise(const std::string& text, std::vector<std::string>& tokens_out)
{
	unsigned int i = 0;

	while(1)
	{
		//-----------------------------------------------------------------
		//read in token
		//-----------------------------------------------------------------

		//-----------------------------------------------------------------
		//eat whitespace
		//-----------------------------------------------------------------
		while(1)
		{
			if(i >= text.length())//eof
				return;

			if(!isWhitespace(text[i]))//got to a non-whitespace char
				break;

			i++;
		}

		//-----------------------------------------------------------------
		//read token
		//-----------------------------------------------------------------
		std::string token;
		while(1)
		{
			if(i >= text.size() || isWhitespace(text[i]))
			{
				//-----------------------------------------------------------------
				//end of token
				//-----------------------------------------------------------------
				tokens_out.push_back(token);
				break;
			}

			token += text[i];
			i++;
		}

	}
}


bool containsString(const std::string& text, const std::string& target_string)
{

	const int lengthdif = text.length() - target_string.length();

	if(lengthdif < 0)
		return false;

	for(int i=0; i<lengthdif; i++)
	{
		if(text.substr(i, target_string.length()) == target_string)
			return true;
	}

	return false;
}


void readInToken(std::istream& stream, std::string& str_out)
{
	//NOTE: untested

	stream >> str_out;

	if(str_out.size() >= 1 && str_out[0] == '\"')
	{
		//-----------------------------------------------------------------
		//keep on reading shit in until we hit another "
		//-----------------------------------------------------------------
		int loopcount = 0;//avoid infinite loops
		while(stream && loopcount < 1000)
		{
			std::string nextword;
			stream >> nextword;

			str_out += " ";
			str_out += nextword;

			if(nextword.size() == 0 || nextword.find_first_of('\"') != std::string::npos)
			{
				break;
			}
		}
	}
}


void readQuote(std::istream& stream, std::string& str_out)//reads string from between double quotes.
{
	stream >> str_out;
	if(str_out.empty() || str_out[0] != '\"')
		return;

	str_out = getTailSubString(str_out, 1);//lop off quote

	if(::hasSuffix(str_out, "\""))
	{
		str_out = str_out.substr(0, str_out.size() - 1);
		return;
	}

	//------------------------------------------------------------------------
	//read thru char by char until hit next quote
	//------------------------------------------------------------------------
	while(stream.good())
	{
		char c;
		stream.get(c);

		if(c == '\"')
			break;

		::concatWithChar(str_out, c);
	}

	/*readInToken(stream, str_out);

	assert(str_out[0] == '\"' && str_out[(int)str_out.size() - 1] == '\"');

	//---------------------------------------------------------------------------
	//lop off the quotes
	//---------------------------------------------------------------------------
	str_out = str_out.substr(1, (int)str_out.size() - 2);*/
}


void writeToQuote(std::ostream& stream, const std::string& str)//writes string to between double quotes.
{
	stream << '\"';
	stream << str;
	stream << '\"';
}


/*unsigned int stringChecksum(const std::string& s)
{
	int sum = 1;
	for(int i=0; i<s.size(); ++i)
	{
		sum *= 1 + (unsigned int)s[i] + i;
	}

	return sum;
}*/


//replaces all occurences of src with dest in string s.
void replaceChar(std::string& s, char src, char dest)
{
	for(unsigned int i=0; i<s.size(); ++i)
		if(s[i] == src)
			s[i] = dest;
}


// Replaces all occurrences of src with dest in string s.
const std::string StringUtils::replaceCharacter(const std::string& s, char src, char dest)
{
	std::string res = s;
	for(unsigned int i=0; i<s.size(); ++i)
		if(res[i] == src)
			res[i] = dest;
	return res;
}


const std::string getTailSubString(const std::string& s, unsigned int first_char_index)
{
	assert(first_char_index >= 0);

	if(first_char_index >= s.size())
		return "";

	return s.substr(first_char_index, s.size() - first_char_index);
}


const std::string forceCopyString(const std::string& s)
{
	std::string newstring;
	newstring.resize(s.size());

	for(int i=0; i<(int)s.size(); ++i)
		newstring[i] = s[i];

	return newstring;
}


const std::string getNiceByteSize(uint64 x)
{
	assert(x >= 0);
	if(x < 1024)
		return ::toString((unsigned int)x) + " B";
	else if(x < 1048576)
	{
		const float kbsize = (float)x / 1024.0f;

		return floatToString(kbsize, 3) + " KB";
	}
	else if(x < 1073741824)
	{
		const float mbsize = (float)x / 1048576.0f;

		return floatToString(mbsize, 3) + " MB";
	}
	else
	{
		const float gbsize = (float)x / 1073741824.0f;

		return floatToString(gbsize, 3) + " GB";
	}
}


const std::string getPrefixBeforeDelim(const std::string& s, char delim)
{
	std::string prefix;
	for(unsigned int i=0; i<s.size(); ++i)
	{
		if(s[i] == delim)
			return prefix;
		else
			::concatWithChar(prefix, s[i]);
	}
	return prefix;
}


const std::vector<std::string> split(const std::string& s, char delim)
{
	std::vector<std::string> parts;
	std::string::size_type start = 0;
	while(1)
	{
		std::string::size_type delimpos = s.find_first_of(delim, start);
		if(delimpos == std::string::npos)
		{
			if(start < s.size())
				parts.push_back(s.substr(start, s.size() - start));
			return parts;
		}

		if(delimpos > start)
			parts.push_back(s.substr(start, delimpos-start));
		start = delimpos + 1;
	}
}


const std::string leftPad(const std::string& s, char c, unsigned int minwidth)
{
	if(minwidth > s.length())
		return std::string(minwidth - s.length(), c) + s;
	else
		return s;
}


const std::string rightPad(const std::string& s, char c, unsigned int minwidth)
{
	if(minwidth > s.length())
		return s + std::string(minwidth - s.length(), c);
	else
		return s;
}


namespace StringUtils
{


void getPosition(const std::string& str, unsigned int charindex, unsigned int& line_num_out, unsigned int& column_out)
{
	assert(charindex < str.size());
	if(charindex >= str.size())
	{
		line_num_out = column_out = 0;
		return;
	}

	unsigned int line = 0;
	unsigned int col = 0;
	for(unsigned int i=0; i<charindex; ++i)
	{
		if(str[i] == '\n')
		{
			line++;
			col = 0;
		}
		else
			col++;
	}

	line_num_out = line;
	column_out = col;
}


#if defined(WIN32) || defined(WIN64)
const std::wstring UTF8ToWString(const std::string& s)
{
	// Call initially to get size of buffer to allocate.
	const int size_required = MultiByteToWideChar(
		CP_UTF8, // code page
		0, // flags
		s.c_str(),
		s.size() + 1, // Byte size of s.  Add one, because we want to include the null terminator.
		NULL, // out buffer
		0 // buffer size
		);

	if(size_required == 0)
		return std::wstring();

	// Call again, this time with the buffer.
	std::vector<wchar_t> buffer(size_required);

	const int result = MultiByteToWideChar(
		CP_UTF8, // code page
		0,
		s.c_str(),
		s.size() + 1, // Add one, because we want to convert the null terminator.
		&buffer[0],
		size_required
		);

	if(result == 0)
	{
		const int er = GetLastError();
		if(er == ERROR_INSUFFICIENT_BUFFER)
		{
			assert(0);
		}
		else if(er == ERROR_INVALID_FLAGS)
		{
			assert(0);
		}
		else if(er == ERROR_INVALID_PARAMETER)
		{
			assert(0);
		}
		else if(er == ERROR_NO_UNICODE_TRANSLATION )
		{
			assert(0);
		}
		//error
		assert(0);
		return std::wstring();
	}

	return std::wstring(buffer.begin(), buffer.end() - 1);
}


const std::string WToUTF8String(const std::wstring& wide_string)
{

	// Call once to get number of bytes required for buffer.
	const int size_required = WideCharToMultiByte(
		CP_UTF8,
		0,
		wide_string.c_str(),
		wide_string.size() + 1,
		NULL,
		0,
		NULL,
		NULL
	);

	std::vector<char> buffer(size_required);

	/*const int num_bytes = */WideCharToMultiByte(
		CP_UTF8,
		0,
		wide_string.c_str(),
		wide_string.size() + 1,
		&buffer[0],
		size_required,
		NULL,
		NULL
	);

	return std::string(buffer.begin(), buffer.end() - 1);
}
#endif


} // end namespace StringUtils


template <class Real>
static inline bool epsEqual(Real a, Real b, Real epsilon = 0.00001f)
{
	return fabs(a - b) <= epsilon;
}


#if (BUILD_TESTS)
void doStringUtilsUnitTests()
{
	testAssert(toString(1234567) == "1234567");
	testAssert(toString(-1234567) == "-1234567");
	testAssert(toString(0) == "0");

	// 64 bit unsigned integer <-> string conversion
	testAssert(toString((uint64)12345671234567LL) == "12345671234567");
	testAssert(toString((uint64)0) == "0");
	testAssert(toString((uint64)1) == "1");
	testAssert(toString((uint64)1234567) == "1234567");
	testAssert(toString((uint64)18446744073709551615ULL) == "18446744073709551615"); // max representable uint64

	testAssert(stringToUInt64("12345671234567") == 12345671234567LL);
	testAssert(stringToUInt64("0") == 0);
	testAssert(stringToUInt64("1") == 1);
	testAssert(stringToUInt64("01") == 1);
	testAssert(stringToUInt64("00001") == 1);
	testAssert(stringToUInt64("10000") == 10000);
	testAssert(stringToUInt64("1234567") == 1234567);
	testAssert(stringToUInt64("18446744073709551615") == 18446744073709551615ULL); // max representable uint64

	try
	{
		stringToUInt64("-1");
		testAssert(!"Shouldn't get here.");
	}
	catch(StringUtilsExcep& e)
	{
		const std::string s = e.what();
	}

	try
	{
		stringToUInt64("18446744073709551616"); // 1 past max representable uint64
		testAssert(!"Shouldn't get here.");
	}
	catch(StringUtilsExcep& e)
	{
		const std::string s = e.what();
	}

	try
	{
		stringToUInt64("1000000000000000000000000000000"); // valmul should overflow
		testAssert(!"Shouldn't get here.");
	}
	catch(StringUtilsExcep& e)
	{
		const std::string s = e.what();
	}


	/*const int N = 100000;
	{
		Timer timer;
		uint64 sumlen = 0;

		for(unsigned i=0; i<N; ++i)
			sumlen += toString(i).size();

		conPrint("toString: " + toString(timer.getSecondsElapsed()));
		conPrint("sumlen: " + toString(sumlen));
	}
	{
		Timer timer;
		uint64 sumlen = 0;

		for(unsigned i=0; i<N; ++i)
			sumlen += toStringStrStream(i).size();

		conPrint("toStringStrStream: " + toString(timer.getSecondsElapsed()));
		conPrint("sumlen: " + toString(sumlen));
	}
	{
		Timer timer;
		uint64 sumlen = 0;

		for(unsigned i=0; i<N; ++i)
			sumlen += toString((uint64)i).size();

		conPrint("toString((uint64): " + toString(timer.getSecondsElapsed()));
		conPrint("sumlen: " + toString(sumlen));
	}*/

#if defined(WIN32) || defined(WIN64)
	// test WToUTF8String and UTF8ToWString
	{
		const int a = sizeof(wchar_t);

		const std::wstring w = L"A";

		const std::string s = StringUtils::WToUTF8String(w);

		assert(s.size() == 1);
		assert(s.c_str()[0] == 'A');
		assert(s.c_str()[1] == '\0');

		const std::wstring w2 = StringUtils::UTF8ToWString(s);

		assert(w == w2);
	}

	// Test with empty string
	{
		const std::wstring w = L"";

		const std::string s = StringUtils::WToUTF8String(w);

		assert(s.size() == 0);

		const std::wstring w2 = StringUtils::UTF8ToWString(s);

		assert(w == w2);
	}


	{
		// This is the Euro sign, encoded in UTF-8: see http://en.wikipedia.org/wiki/UTF-8
		const std::string s = "\xE2\x82\xAC";

		assert(s.size() == 3);

		const std::wstring w = StringUtils::UTF8ToWString(s);

		const std::string s2 = StringUtils::WToUTF8String(w);

		assert(s == s2);
	}
#endif



	std::vector<std::string> parts = split("a:b:c", ':');
	assert(parts.size() == 3);
	assert(parts[0] == "a");
	assert(parts[1] == "b");
	assert(parts[2] == "c");

	parts = split("a:b:", ':');
	assert(parts.size() == 2);
	assert(parts[0] == "a");
	assert(parts[1] == "b");

	parts = split(":a:b:", ':');
	assert(parts.size() == 2);
	assert(parts[0] == "a");
	assert(parts[1] == "b");

	parts = split(":a:b", ':');
	assert(parts.size() == 2);
	assert(parts[0] == "a");
	assert(parts[1] == "b");

	assert(rightPad("123", 'a', 5) == "123aa");
	assert(rightPad("12345", 'a', 3) == "12345");



	parts = split("a:b:c", ':');
	assert(StringUtils::join(parts, ":") == "a:b:c");

	parts = split("a", ':');
	assert(StringUtils::join(parts, ":") == "a");

	parts = split("", ':');
	assert(StringUtils::join(parts, ":") == "");


	// Test float to string
	{
		testAssert(::epsEqual(::stringToFloat(::floatToString(123.456f)), 123.456f));

		testAssert(::floatToString(123.456f, 0) == "123");
		testAssert(::floatToString(123.234f, 1) == "123.2");
		testAssert(::floatToString(123.234f, 2) == "123.23");
		testAssert(::floatToString(123.234f, 3) == "123.234");
	}
	// Try German locale where decimal separtor is ','
	{
		const char* result = std::setlocale(LC_ALL, "german");
		if(result)
		{
			// WIll only work if german locale is installed.
			// Run locale -a on console to view installed locales.

			testAssert(::floatToString(123.234f, 1) == "123,2");
	
			testAssert(::epsEqual(::stringToFloat("123,456"), 123.456f));
		}
	}

	// Reset Locale
	{
		const char* result = std::setlocale(LC_ALL, "");
		testAssert(result != NULL);
	}


	// Test string to float
	assert(epsEqual(stringToFloat("1.4"), 1.4f));

	try
	{
		stringToFloat("bleh");
		assert(false);
	}
	catch(StringUtilsExcep& )
	{}

	assert(epsEqual(stringToDouble("-631.3543e52"), -631.3543e52));

	try
	{
		stringToFloat("-z631.3543");
		assert(false);
	}
	catch(StringUtilsExcep& )
	{}

	try
	{
		stringToFloat("");
		assert(false);
	}
	catch(StringUtilsExcep& )
	{}

	try
	{
		stringToFloat(" ");
		assert(false);
	}
	catch(StringUtilsExcep& )
	{}

	assert(epsEqual((double)stringToInt("-631"), (double)-631));


	try
	{
		stringToInt("dfgg");
		assert(false);
	}
	catch(StringUtilsExcep& )
	{}


	{
		const std::string s = "\n\
			line1\n\
			line2\n\
			line3";

		unsigned int line, col;
		StringUtils::getPosition(s, 4, line, col);
		assert(line == 1 && col == 3);
	}



	assert(::toUpperCase("meh666XYZ") == "MEH666XYZ");

	assert(::toLowerCase("meh666XYZ") == "meh666xyz");

	assert(::toLowerCase('C') == 'c');
	assert(::toLowerCase('1') == '1');
	assert(::toLowerCase('c') == 'c');

	assert(::toUpperCase('C') == 'C');
	assert(::toUpperCase('1') == '1');
	assert(::toUpperCase('c') == 'C');

	assert(getPrefixBeforeDelim("meh666", '6') == "meh");
	assert(getPrefixBeforeDelim("meh666", '3') == "meh666");
	assert(getPrefixBeforeDelim("meh666", 'm') == "");

	assert(::toHexString(0x34bc8106) == "34BC8106");
	assert(::toHexString(0x0) == "0");
	assert(::toHexString(0x00000) == "0");
	assert(::toHexString(0x1) == "1");
	assert(::toHexString(0x00000001) == "1");
	assert(::toHexString(0xFFFFFFFF) == "FFFFFFFF");
	assert(::toHexString(0xA4) == "A4");

	assert(::hexStringToUInt("0x34bc8106") == 0x34bc8106);
	assert(::hexStringToUInt("0x0005F") == 0x0005F);
	assert(::hexStringToUInt("0x0") == 0x0);
	assert(::hexStringToUInt("skjhsdg") == 0);//parse error


	assert(::toString('a') == std::string("a"));
	assert(::toString(' ') == std::string(" "));

	assert(::hasSuffix("test", "st"));
	assert(::hasSuffix("test", "t"));
	assert(::hasSuffix("test", "est"));
	assert(::hasSuffix("test", "test"));
	assert(::hasSuffix("test", ""));

	assert(::isWhitespace(' '));
	assert(::isWhitespace('\t'));
	assert(::isWhitespace('	'));
/*
	{
	const std::string a = ::eatTailWhitespace("abc  \t    ");
	assert(a == "abc");
	const std::string b = ::eatTailWhitespace("    ");
	assert(b == "");
	const std::string c = ::eatTailWhitespace("");
	assert(c == "");
	}

	{
	const std::string a = ::eatPrefix("abc", "a");
	assert(a == "bc");

	const std::string b = ::eatPrefix("abc", "bc");
	assert(b == "abc");

	const std::string c = ::eatPrefix(" jkl", " j");
	assert(c == "kl");

	const std::string d = ::eatSuffix(" jkl ", "jkl ");
	assert(d == " ");

	const std::string e = ::eatSuffix("", "");
	assert(e == "");

	}

	assert(getTailSubString("abc", 0) == "abc");
	assert(getTailSubString("abc", 1) == "bc");
	assert(getTailSubString("abc", 2) == "c");
	assert(getTailSubString("abc", 3) == "");
	assert(getTailSubString("abc", 4) == "");

	const int testint = 42;
	assert(writeToStringStream(testint) == "42");
	assert(writeToStringStream("bla") == "bla");

	const std::string teststring = "42";
	int resultint;
	readFromStringStream(teststring, resultint);
	assert(resultint == 42);

*/
	/*assert(::concatWithChar("abc", 'd') == "abcd");
	assert(::concatWithChar("", 'a') == "a");
	assert(::concatWithChar("aa", '_') == "aa_");*/

}
#endif

