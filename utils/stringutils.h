/*===================================================================
Copyright Glare Technologies Limited 2009 -
====================================================================*/
#ifndef __STRINGUTILS_H__
#define __STRINGUTILS_H__


//NOTE: not all of this code has been used/tested for ages.
//Your mileage may vary; test the code before you use it!!!!

#include "../utils/platform.h"
#include <string>
#include <assert.h>
#include <vector>
#include <sstream>


class StringUtilsExcep
{
public:
	StringUtilsExcep(const std::string& s_) : s(s_) {}
	~StringUtilsExcep(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};


float stringToFloat(const std::string& s); // throws StringUtilsExcep
double stringToDouble(const std::string& s); // throws StringUtilsExcep
int stringToInt(const std::string& s); // throws StringUtilsExcep
uint64 stringToUInt64(const std::string& s); // throws StringUtilsExcep


inline const std::string toString(char c)
{
	return std::string(1, c);
}


unsigned int hexCharToUInt(char c);
unsigned int hexStringToUInt(const std::string& s);
unsigned long long hexStringTo64UInt(const std::string& s);

//const std::string toHexString(unsigned int i);//32 bit integers
const std::string toHexString(unsigned long long i);//for 64 bit integers
const std::string intToString(int i);
const std::string floatToString(float f);
const std::string doubleToString(double d, int num_decimal_places = 5);
const std::string doubleToStringScientific(double d, int num_decimal_places = 5);
const std::string floatToString(float f, int num_decimal_places);

//argument overloaded toString functions:
inline const std::string toString(double f)
{
	return doubleToString(f);
}

inline const std::string toString(float f)
{
	return floatToString(f);
}

inline const std::string toString(int i)
{
	return intToString(i);
}

const std::string toString(unsigned int x);

const std::string toString(uint64 x);
const std::string toString(int64 x);


const std::string boolToString(bool b);

//get line, where the first line is line number 1!
const std::string getLineFromText(int linenum, const char* textbuffer, int textbufferlength);

inline bool isWhitespace(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

inline bool isNumeric(char c)
{
	return c >= '0' && c <= '9';
}

inline bool isAlphabetic(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

inline bool isAlphaNumeric(char c)
{
	return isNumeric(c) || isAlphabetic(c);
}

const std::string eatHeadWhitespace(const std::string& text);
const std::string eatTailWhitespace(const std::string& text);
const std::string eatWhitespace(const std::string& s);

const std::string toLowerCase(const std::string& text);
const std::string toUpperCase(const std::string& text);

char toLowerCase(char c);
char toUpperCase(char c);

bool hasFileTypeExtension(const std::string& filename);
const std::string getExtension(const std::string& filename);//returns 3 letter extension

const std::string eatExtension(const std::string& filename);
const std::string eatDotAndExtension(const std::string& filename);

// Without the dot
bool hasExtension(const std::string& filename, const std::string& extension);

bool hasPrefix(const std::string& s, const std::string& prefix);
bool hasSuffix(const std::string& s, const std::string& suffix);

const std::string eatPrefix(const std::string& s, const std::string& prefix);
const std::string eatSuffix(const std::string& s, const std::string& suffix);

int getNumMatches(const std::string& s, char target);

void tokenise(const std::string& text, std::vector<std::string>& tokens_out);

bool containsString(const std::string& text, const std::string& target_string);

// Can handle multiple words in quotes
void readInToken(std::istream& stream, std::string& str_out);

void readQuote(std::istream& stream, std::string& str_out); // Reads string from between double quotes.
void writeToQuote(std::ostream& stream, const std::string& str); // Writes string to between double quotes.

unsigned int stringChecksum(const std::string& s);


template <class T>
inline const std::string writeToStringStream(T& t)
{
	std::ostringstream outstream;
	outstream << t;

	return outstream.str();
}

template <class T>
inline void readFromStringStream(const std::string& instring, T& t_out)
{
	std::istringstream instream(instring);
	instream >> t_out;
}


// Replaces all occurences of src with dest in string s.
void replaceChar(std::string& s, char src, char dest);

inline void concatWithChar(std::string& s, char c)
{
	s = s + std::string(1, c);
}

inline const std::string appendChar(const std::string& s, char c)
{
	return s + std::string(1, c);
}

// If first_char_index is >= s.size(), then returns ""
const std::string getTailSubString(const std::string& s, unsigned int first_char_index);

const std::string forceCopyString(const std::string& s);

// Returns as 4.6MB etc.. instead of 46287567B
const std::string getNiceByteSize(uint64 x);

const std::string getPrefixBeforeDelim(const std::string& s, char delim);

const std::vector<std::string> split(const std::string& s, char delim);

const std::string leftPad(const std::string& s, char c, unsigned int minwidth);
const std::string rightPad(const std::string& s, char c, unsigned int minwidth);

namespace StringUtils
{
template<class T>
const std::string join(const T& iterable, const std::string joinstring)
{
	std::string s;
	bool first_elem = true;
	for(typename T::const_iterator i = iterable.begin(); i != iterable.end(); ++i)
	{
		s = s + (first_elem ? std::string("") : joinstring) + (*i);
		first_elem = false;
	}
	return s;
}

// Returns 0-based index of line and column of character indexed by charindex
void getPosition(const std::string& str, unsigned int charindex, unsigned int& line_num_out, unsigned int& column_out);


#if defined(WIN32) || defined(WIN64)
const std::wstring UTF8ToWString(const std::string& s);
const std::string WToUTF8String(const std::wstring& s);
#endif

 //  && !defined(__MINGW32__)
#if (defined(WIN32) || defined(WIN64))
inline const std::wstring UTF8ToPlatformUnicodeEncoding(const std::string& s) { return UTF8ToWString(s); }
inline const std::string PlatformToUTF8UnicodeEncoding(const std::wstring& s) { return WToUTF8String(s); }
#else
inline const std::string UTF8ToPlatformUnicodeEncoding(const std::string& s) { return s; }
inline const std::string PlatformToUTF8UnicodeEncoding(const std::string& s) { return s; }
#endif


// Replaces all occurrences of src with dest in string s.
const std::string replaceCharacter(const std::string& s, char src, char dest);



}; // end namespace StringUtils


//NOTE: must be in debug mode for this to work
void doStringUtilsUnitTests();


#endif //__STRINGUTILS_H__
