/*=====================================================================
StringUtils.h
-------------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "string_view.h"
#include <string>
#include <vector>
#include <string.h> // for strcmp()


class StringUtilsExcep
{
public:
	StringUtilsExcep(const std::string& s_) : s(s_) {}
	~StringUtilsExcep(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};


//====================== String to Number conversion ======================
// These functions ignore the current locale.  Decimal seperator is always considered to be '.'.

float stringToFloat(const std::string& s); // throws StringUtilsExcep
double stringToDouble(const std::string& s); // throws StringUtilsExcep

int stringToInt(const std::string& s); // throws StringUtilsExcep
uint64 stringToUInt64(const std::string& s); // throws StringUtilsExcep

unsigned int hexCharToUInt(char c); // throws StringUtilsExcep
uint32 hexStringToUInt32(const std::string& s); // Input: something like "AABBCCDD"
uint64 hexStringTo64UInt(const std::string& s);

//====================== Number to String conversion ======================
//const std::string toHexString(unsigned int i);//32 bit integers

char intToHexChar(int i); // throws StringUtilsExcep

const std::string toHexString(uint64 i); // Returns a string like "F4B350".  The returned string will not have leading zeros.

const std::string int32ToString(int32 i);
const std::string int64ToString(int64 i);
const std::string uInt32ToString(uint32 x);
const std::string uInt64ToString(uint64 x);

// These functions write the shortest string such that they can be re-read to get the original number.
const std::string floatToString(float f);
const std::string doubleToString(double d);

// Write to a string with a certain number of decimal places.  Slow.
const std::string floatToStringNDecimalPlaces(float f, int num_decimal_places);
const std::string doubleToStringNDecimalPlaces(double d, int num_decimal_places);

const std::string floatToStringNSigFigs(float f, int num_sig_figs);
const std::string doubleToStringNSigFigs(double d, int num_sig_figs);

// Write to a string with scientific notation, e.g. 4.0e10.
const std::string doubleToStringScientific(double d, int num_decimal_places = 5);

// Returns in form x.f or x.yf, e.g instead of returning 2 it returns 2.f
const std::string floatLiteralString(float x);

// Returns in form x. or x.y, e.g instead of returning 2 it returns 2.
const std::string doubleLiteralString(double x);


// Overloaded toString functions:
inline const std::string toString(double f)
{
	return doubleToString(f);
}

inline const std::string toString(float f)
{
	return floatToString(f);
}

inline const std::string toString(int32 i)
{
	return int32ToString(i);
}

inline const std::string toString(int64 i)
{
	return int64ToString(i);
}

inline const std::string toString(uint32 x)
{
	return uInt32ToString(x);
}

inline const std::string toString(uint64 x)
{
	return uInt64ToString(x);
}


#ifdef OSX
const std::string toString(size_t x);
#endif

//====================== End Number -> String conversion ======================

inline const std::string charToString(char c)
{
	return std::string(1, c);
}

const std::string boolToString(bool b);

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

const std::string stripHeadWhitespace(const std::string& text);
const std::string stripTailWhitespace(const std::string& text);
const std::string stripHeadAndTailWhitespace(const std::string& text);
const std::string stripWhitespace(const std::string& s); // Strip all whitespace, included interior whitespace.
const std::string collapseWhitespace(const std::string& s); // Convert runs of 1 or more whitespace characters to just the first whitespace char.
bool isAllWhitespace(const std::string& s);

const std::string toLowerCase(const std::string& text);
const std::string toUpperCase(const std::string& text);

char toLowerCase(char c);
char toUpperCase(char c);

const std::string getExtension(const std::string& filename); // Returns everything after the last dot.

const std::string eatExtension(const std::string& filename);
const std::string removeDotAndExtension(const std::string& filename);

// Without the dot
bool hasExtension(const std::string& filename, const std::string& extension);

bool hasPrefix(const std::string& s, const string_view& prefix);
bool hasSuffix(const std::string& s, const string_view& suffix);
bool hasLastChar(const std::string& s, char c);

const std::string eatPrefix(const std::string& s, const std::string& prefix);
const std::string eatSuffix(const std::string& s, const std::string& suffix);

int getNumMatches(const std::string& s, char target);

// Replaces all occurences of src with dest in string s.
void replaceChar(std::string& s, char src, char dest);

// If first_char_index is >= s.size(), then returns ""
const std::string getTailSubString(const std::string& s, size_t first_char_index);

const std::string forceCopyString(const std::string& s);

// Returns as 4.6 MB etc.. instead of 46287567B
const std::string getNiceByteSize(uint64 x);

const std::string getPrefixBeforeDelim(const std::string& s, char delim);
const std::string getSuffixAfterDelim(const std::string& s, char delim);

const std::vector<std::string> split(const std::string& s, char delim);

const std::string leftPad(const std::string& s, char c, unsigned int minwidth);
const std::string rightPad(const std::string& s, char c, unsigned int minwidth);
const std::string rightSpacePad(const std::string& s, unsigned int minwidth);

static inline bool stringEqual(const char* a, const char* b)
{
	return ::strcmp(a, b) == 0;
}


namespace StringUtils
{


template<class T>
const std::string join(const T& iterable, const std::string& joinstring)
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
void getPosition(const std::string& str, size_t charindex, size_t& line_num_out, size_t& column_out);
const std::string getLineFromBuffer(const std::string& str, size_t charindex);


#if defined(_WIN32)
const std::wstring UTF8ToWString(const std::string& s);
const std::string WToUTF8String(const std::wstring& s);
#endif

#if defined(_WIN32)
inline const std::wstring UTF8ToPlatformUnicodeEncoding(const std::string& s) { return UTF8ToWString(s); }
inline const std::string PlatformToUTF8UnicodeEncoding(const std::wstring& s) { return WToUTF8String(s); }
#else
inline const std::string UTF8ToPlatformUnicodeEncoding(const std::string& s) { return s; }
inline const std::string PlatformToUTF8UnicodeEncoding(const std::string& s) { return s; }
#endif


// Replaces all occurrences of src with dest in string s.
const std::string replaceCharacter(const std::string& s, char src, char dest);

const std::vector<unsigned char> convertHexToBinary(const std::string& hex);
const std::string convertByteArrayToHexString(const std::vector<unsigned char>& bytes);

const std::vector<unsigned char> stringToByteArray(const std::string& s);
const std::string byteArrayToString(const std::vector<unsigned char>& bytes);


const std::string replaceFirst(const std::string& s, const std::string& target, const std::string& replacement);

const std::string replaceAll(const std::string& s, const std::string& target, const std::string& replacement);

void appendTabbed(std::string& s, const std::string& addition, int num_tabs);

bool containsChar(const std::string& s, char c);
bool containsString(const std::string& s, const std::string& target);

// Replace non-printable chars with '?'
const std::string removeNonPrintableChars(const std::string& s);

const std::vector<std::string> splitIntoLines(const std::string& s);

void test();


}; // end namespace StringUtils
