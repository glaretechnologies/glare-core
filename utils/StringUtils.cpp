/*=====================================================================
StringUtils.cpp
-------------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "StringUtils.h"


#include "IncludeWindows.h"
#include "BitUtils.h"
#include "../maths/mathstypes.h"
#include "../double-conversion/double-conversion.h"
#include <cmath>
#include <cstddef>
#include <stdlib.h>
#include <stdio.h>
#include <limits>


float stringToFloat(const std::string& s) // throws StringUtilsExcep
{
	/*char* end_ptr = NULL;
	const float ret = (float)strtod(s.c_str(), &end_ptr);
	if(end_ptr == s.c_str())
		throw StringUtilsExcep("Failed to convert '" + s + "' to a float.");
	return ret;*/

	double_conversion::StringToDoubleConverter s2d_converter(
		double_conversion::StringToDoubleConverter::NO_FLAGS,
		std::numeric_limits<float>::quiet_NaN(), // empty string value
		std::numeric_limits<float>::quiet_NaN(), // junk string value.  We'll use this to detect failed parses.
		NULL, // infinity symbol
		NULL // NaN symbol
	);

	int num_processed_chars = 0;
	const float x = s2d_converter.StringToFloat(s.c_str(), (int)s.length(), &num_processed_chars);
	if(::isNAN(x))
		throw StringUtilsExcep("Failed to convert '" + s + "' to a float.");
	return x;
}


double stringToDouble(const std::string& s) // throws StringUtilsExcep
{
	/*char* end_ptr = NULL;
	const double ret = strtod(s.c_str(), &end_ptr);
	if(end_ptr == s.c_str())
		throw StringUtilsExcep("Failed to convert '" + s + "' to a double.");
	return ret;*/

	double_conversion::StringToDoubleConverter s2d_converter(
		double_conversion::StringToDoubleConverter::NO_FLAGS,
		std::numeric_limits<double>::quiet_NaN(), // empty string value
		std::numeric_limits<double>::quiet_NaN(), // junk string value.  We'll use this to detect failed parses.
		NULL, // infinity symbol
		NULL // NaN symbol
	);

	int num_processed_chars = 0;
	const double x = s2d_converter.StringToDouble(s.c_str(), (int)s.length(), &num_processed_chars);
	if(::isNAN(x))
		throw StringUtilsExcep("Failed to convert '" + s + "' to a double.");
	return x;
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
		throw StringUtilsExcep("Character " + std::string(1, c) + " is not a valid hex character.");
}


uint32 hexStringToUInt32(const std::string& s)
{
	if(s.empty())
		throw StringUtilsExcep("Failed to convert '" + s + "', string was empty.");
	else if(s.size() > 8)
		throw StringUtilsExcep("Failed to convert '" + s + "', string was too long.");

	uint32 x = 0;
	for(size_t i=0; i<s.size(); ++i)
	{
		uint32 nibble;
		if(s[i] >= '0' && s[i] <= '9')
			nibble = s[i] - '0';
		else if(s[i] >= 'a' && s[i] <= 'f')
			nibble = s[i] - 'a' + 10;
		else if(s[i] >= 'A' && s[i] <= 'F')
			nibble = s[i] - 'A' + 10;
		else
			throw StringUtilsExcep("Invalid character '" + std::string(1, s[i]) + "'.");

		x <<= 4;
		x |= nibble; // Set lower 4 bits to nibble
	}

	return x;
}


uint64 hexStringToUInt64(const std::string& s)
{
	if(s.empty())
		throw StringUtilsExcep("Failed to convert '" + s + "', string was empty.");
	else if(s.size() > 16)
		throw StringUtilsExcep("Failed to convert '" + s + "', string was too long.");

	uint64 x = 0;
	for(size_t i=0; i<s.size(); ++i)
	{
		uint64 nibble;
		if(s[i] >= '0' && s[i] <= '9')
			nibble = s[i] - '0';
		else if(s[i] >= 'a' && s[i] <= 'f')
			nibble = s[i] - 'a' + 10;
		else if(s[i] >= 'A' && s[i] <= 'F')
			nibble = s[i] - 'A' + 10;
		else
			throw StringUtilsExcep("Invalid character '" + std::string(1, s[i]) + "'.");

		x <<= 4;
		x |= nibble; // Set lower 4 bits to nibble
	}

	return x;
}


char intToHexChar(int i)
{
	if(i < 0)
		throw StringUtilsExcep("value out of range.");
	else if(i <= 9)
		return '0' + (char)i;
	else if(i < 16)
		return 'A' + (char)(i - 10);
	else
		throw StringUtilsExcep("value out of range.");
}


static const char hextable[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

// Returns a string like "F4B350".  The returned string will not have leading zeros.
const std::string toHexString(uint64 x)
{
	char buf[16];

	int leftmost_nonzero_digit_i;
	if(x == 0)
		leftmost_nonzero_digit_i = 15;
	else
	{
		const int bitindex = BitUtils::highestSetBitIndex(x);
		const int nibble_index = bitindex >> 2;
		leftmost_nonzero_digit_i = 15 - nibble_index;
	}
	assert(leftmost_nonzero_digit_i >= 0 && leftmost_nonzero_digit_i < 16);

	for(int i=leftmost_nonzero_digit_i; i<16; ++i)
	{
		const uint64 nibble = (x >> (60 - (i*4))) & 0xFull; // Get 4 bits at offset i*4 from left.
		buf[i] = hextable[nibble];
	}
	return std::string(buf + leftmost_nonzero_digit_i, buf + 16);
}


const std::string floatToString(float f)
{
	// Due to fast maths optimisations with VS that treat -0 like 0, a float value of -0.0 will sometimes get passed in to this function instead of 0.0.
	// We want to print "0" for this, so handle this case explicitly here.
	if(f == 0 && !isNAN(f))
		return "0";

	// Convert double to string with Google's code
	double_conversion::DoubleToStringConverter converter(
		double_conversion::DoubleToStringConverter::NO_FLAGS,
		"Inf", // Infinity symbol
		"NaN", // NaN symbol
		'e',
		-6, // decimal_in_shortest_low
		21, // decimal_in_shortest_high
		0, // max_leading_padding_zeroes_in_precision_mode
		0 // max_trailing_padding_zeroes_in_precision_mode
	);

	char buffer[64];
	double_conversion::StringBuilder builder(buffer, sizeof(buffer));

	converter.ToShortestSingle(f, &builder);

	return std::string(builder.Finalize());
}


const std::string doubleToString(double d)
{
	// Due to fast maths optimisations with VS that treat -0 like 0, a float value of -0.0 will sometimes get passed in to this function instead of 0.0.
	// We want to print "0" for this, so handle this case explicitly here.
	if(d == 0 && !isNAN(d))
		return "0";

	// Convert double to string with Google's code
	double_conversion::DoubleToStringConverter converter(
		double_conversion::DoubleToStringConverter::NO_FLAGS,
		"Inf", // Infinity symbol
		"NaN", // NaN symbol
		'e',
		-6, // decimal_in_shortest_low
		21, // decimal_in_shortest_high
		0, // max_leading_padding_zeroes_in_precision_mode
		0 // max_trailing_padding_zeroes_in_precision_mode
	);

	char buffer[128];
	double_conversion::StringBuilder builder(buffer, sizeof(buffer));

	converter.ToShortest(d, &builder);

	return std::string(builder.Finalize());
}


const std::string doubleToStringNDecimalPlaces(double d, int num_decimal_places)
{
	assert(num_decimal_places >= 0 && num_decimal_places <= 100);

	char buffer[128];

	sprintf(buffer, std::string("%1." + toString(num_decimal_places) + "f").c_str(), d);

	return std::string(buffer);
}


const std::string doubleToStringScientific(double d, int num_decimal_places)
{
	assert(num_decimal_places >= 0 && num_decimal_places <= 100);
	
	char buffer[512];
	
	sprintf(buffer, std::string("%1." + toString(num_decimal_places) + "E").c_str(), d);
	
	return std::string(buffer);
}


const std::string floatToStringNDecimalPlaces(float f, int num_decimal_places)
{
	assert(num_decimal_places >= 0);

	char buffer[100];

	if(num_decimal_places >= 10)
		num_decimal_places = 9;

	const std::string format_string = "%1." + ::toString(num_decimal_places) + "f";

	sprintf(buffer, format_string.c_str(), f);

	return std::string(buffer);
}


const std::string floatToStringNSigFigs(float f, int num_sig_figs)
{
	// Due to fast maths optimisations with VS that treat -0 like 0, a float value of -0.0 will sometimes get passed in to this function instead of 0.0.
	// We want to print "0" for this, so handle this case explicitly here.
	if(f == 0 && !isNAN(f))
		return "0";

	// Convert double to string with Google's code
	double_conversion::DoubleToStringConverter converter(
		double_conversion::DoubleToStringConverter::NO_FLAGS,
		"Inf", // Infinity symbol
		"NaN", // NaN symbol
		'e',
		-6, // decimal_in_shortest_low - not used for precision mode
		21, // decimal_in_shortest_high - not used for precision mode
		10, // max_leading_padding_zeroes_in_precision_mode - set this high to avoid formatting in exponential mode (e.g. 1.0e-8)
		10 // max_trailing_padding_zeroes_in_precision_mode - set this high to avoid formatting in exponential mode (e.g. 1.0e8)
	);

	// The result will never contain more than kMaxPrecisionDigits (=120) + 7 characters
	char buffer[128];
	double_conversion::StringBuilder builder(buffer, sizeof(buffer));

	// Sig figs has to be in this range for ToPrecision() to succeed.
	const int use_sig_figs = myClamp(num_sig_figs, double_conversion::DoubleToStringConverter::kMinPrecisionDigits, double_conversion::DoubleToStringConverter::kMaxPrecisionDigits);

	const bool res = converter.ToPrecision(f, use_sig_figs, &builder);
	assertOrDeclareUsed(res);

	return std::string(builder.Finalize());
}


const std::string doubleToStringNSigFigs(double f, int num_sig_figs)
{
	// Due to fast maths optimisations with VS that treat -0 like 0, a float value of -0.0 will sometimes get passed in to this function instead of 0.0.
	// We want to print "0" for this, so handle this case explicitly here.
	if(f == 0 && !isNAN(f))
		return "0";

	// Convert double to string with Google's code
	double_conversion::DoubleToStringConverter converter(
		double_conversion::DoubleToStringConverter::NO_FLAGS,
		"Inf", // Infinity symbol
		"NaN", // NaN symbol
		'e',
		-6, // decimal_in_shortest_low - not used for precision mode
		21, // decimal_in_shortest_high - not used for precision mode
		10, // max_leading_padding_zeroes_in_precision_mode - set this high to avoid formatting in exponential mode (e.g. 1.0e-8)
		10 // max_trailing_padding_zeroes_in_precision_mode - set this high to avoid formatting in exponential mode (e.g. 1.0e8)
	);

	// The result will never contain more than kMaxPrecisionDigits (=120) + 7 characters
	char buffer[128];
	double_conversion::StringBuilder builder(buffer, sizeof(buffer));

	// Sig figs has to be in this range for ToPrecision() to succeed.
	const int use_sig_figs = myClamp(num_sig_figs, double_conversion::DoubleToStringConverter::kMinPrecisionDigits, double_conversion::DoubleToStringConverter::kMaxPrecisionDigits);

	const bool res = converter.ToPrecision(f, use_sig_figs, &builder);
	assertOrDeclareUsed(res);

	return std::string(builder.Finalize());
}


// Returns in form x.f or x.yf, e.g instead of returning 2 it returns 2.f
const std::string floatLiteralString(float x)
{
	if(isNAN(x))
		return "NaN";
	else if(x == std::numeric_limits<float>::infinity())
		return "Inf";
	else if(x == -std::numeric_limits<float>::infinity())
		return "-Inf";

	const std::string s = floatToString(x);

	// Make sure we have a 'f' suffix on our float literals.
	if(StringUtils::containsChar(s, 'e')) // Scientific notation: something like "1e-30".
		return s + "f"; // e.g '2.3e8' -> '2.3e8f'

	if(StringUtils::containsChar(s, '.'))
		return s + "f"; // e.g '2.3' -> '2.3f'
	else
		return s + ".f"; // e.g. '2'  ->  '2.f'
}


// Returns in form x. or x.y, e.g instead of returning 2 it returns 2.
const std::string doubleLiteralString(double x)
{
	if(isNAN(x))
		return "NaN";
	else if(x == std::numeric_limits<double>::infinity())
		return "Inf";
	else if(x == -std::numeric_limits<double>::infinity())
		return "-Inf";

	const std::string s = doubleToString(x);

	if(StringUtils::containsChar(s, 'e')) // Scientific notation: something like "1e-30".
		return s;

	if(!StringUtils::containsChar(s, '.'))
		return s + "."; // e.g. '2'  ->  '2.'
	else
		return s;
}


// Returns the number of digits in the base-10 representation of x.
// NOTE: Max uint32 is 4294967295. (10 digits)
static int numDigitsUInt32(uint32 x)
{
	// Let d(x) be the number of digits in the base-10 representation of x.
	if(x < 10000u) // if d(x) < 5
	{
		if(x < 100u) // if d(x) < 3
			return x < 10u ? 1 : 2;
		else // else if d(x) >= 3
			return x < 1000u ? 3 : 4;
	}
	else // else if d(x) >= 5
	{
		if(x < 10000000u) // if d(x) < 8  (d(x) e {5, 6, 7})
			return x < 100000u ? 5 : (x < 1000000u ? 6 : 7);
		else // else if d(x) >= 8   (d(x) e {8, 9, 10})
			return x < 100000000u ? 8 : (x < 1000000000u ? 9 : 10);
	}
}


// Returns the number of digits in the base-10 representation of x.
// NOTE: Max uint64 is 18446744073709551615. (20 digits)
static int numDigitsUInt64(uint64 x)
{
	// Let d(x) be the number of digits in the base-10 representation of x.
	if(x < 1000000000u) // if d(x) < 10
	{
		if(x < 10000u) // if d(x) < 5
		{
			if(x < 100) // if d(x) < 3
				return x < 10u ? 1 : 2;
			else // else d(x) >= 3
				return x < 1000u ? 3 : 4;
		}
		else // else if d(x) >= 5
		{
			if(x < 1000000u) // if d(x) < 7
				return x < 100000u ? 5 : 6;
			else // else if d(x) >= 7
				return x < 10000000u ? 7 : (x < 100000000u ? 8 : 9);
		}
	}
	else // else if d(x) >= 10
	{
		if(x < 100000000000000ull) // if d(x) < 15
		{
			if(x < 1000000000000ull) // if d(x) < 13
				return x < 10000000000ull ? 10 : (x < 100000000000ull ? 11 : 12);
			else // else d(x) >= 13
				return x < 10000000000000ull ? 13 : 14;
		}
		else // else if d(x) >= 15
		{
			if(x < 100000000000000000ull) // if d(x) < 18
				return x < 1000000000000000ull ? 15 : (x < 10000000000000000ull ? 16 : 17);
			else // // if d(x) >= 18
				return x < 1000000000000000000ull ? 18 : (x < 10000000000000000000ull ? 19 : 20);
		}
	}
}


const std::string uInt32ToString(uint32 x)
{
	const int num_digits = numDigitsUInt32(x);
	char buffer[16];
	int i = num_digits - 1;

	while(i >= 0)
	{
		const uint32 x_div_10 = x / 10u;
		const uint32 last_digit = x - x_div_10*10;
		buffer[i] = '0' + (char)last_digit;
		i--;
		x = x_div_10;
	}

	return std::string(buffer, buffer + num_digits);
}


const std::string uInt64ToString(uint64 x)
{
	const int num_digits = numDigitsUInt64(x);
	char buffer[32];
	int i = num_digits - 1;

	while(i >= 0)
	{
		const uint64 x_div_10 = x / 10u;
		const uint64 last_digit = x - x_div_10*10;
		buffer[i] = '0' + (char)last_digit;
		i--;
		x = x_div_10;
	}

	return std::string(buffer, buffer + num_digits);
}


const std::string referenceInt32ToString(int32 i)
{
	char buffer[16];

	sprintf(buffer, "%i", i);

	return std::string(buffer);
}


const std::string int32ToString(int32 i)
{
	if(i >= 0)
		return uInt32ToString((uint32)i);
	else
	{
		if(i == std::numeric_limits<int32>::min()) // -2147483648 can't be represented as a positive integer.
		{
			return "-2147483648";
		}
		else
		{
			// Write out the number string in a similar way as for toString(uint32), but leave space for a leading '-' character.
			uint32 x = (uint32)(-i);
			const int num_digits = numDigitsUInt32(x);
			char buffer[16];
			int d_i = num_digits;

			while(d_i >= 1)
			{
				const uint32 x_div_10 = x / 10u;
				const uint32 last_digit = x - x_div_10*10;
				buffer[d_i] = '0' + (char)last_digit;
				d_i--;
				x = x_div_10;
			}

			buffer[0] = '-';

			return std::string(buffer, buffer + num_digits + 1);
		}
	}
}


const std::string int64ToString(int64 i)
{
	if(i >= 0)
		return uInt64ToString((uint64)i);
	else
	{
		if(i == std::numeric_limits<int64>::min()) // -9223372036854775808 can't be represented as a positive int64.
		{
			return "-9223372036854775808";
		}
		else
		{
			// Write out the number string in a similar way as for toString(uint64), but leave space for a leading '-' character.
			uint64 x = (uint64)(-i);
			const int num_digits = numDigitsUInt64(x);
			char buffer[32];
			int d_i = num_digits;

			while(d_i >= 1)
			{
				const uint64 x_div_10 = x / 10u;
				const uint64 last_digit = x - x_div_10*10;
				buffer[d_i] = '0' + (char)last_digit;
				d_i--;
				x = x_div_10;
			}

			buffer[0] = '-';

			return std::string(buffer, buffer + num_digits + 1);
		}
	}
}


#ifdef OSX
const std::string toString(size_t x)
{
	if(sizeof(size_t) == 4)
		return toString((uint32)x);
	else
		return toString((uint64)x);
}
#endif


const std::string boolToString(bool b)
{
	return b ? "true" : "false";
}


const std::string stripHeadWhitespace(const std::string& text)
{
	for(size_t i=0; i<text.size(); i++)
		if(!isWhitespace(text[i]))
			return text.substr(i, text.size() - i); // Non-whitespace starts here

	// If got here, everything was whitespace.
	return std::string();
}


const std::string stripTailWhitespace(const std::string& text)
{
	for(ptrdiff_t i=(ptrdiff_t)text.size() - 1; i>=0; --i) // Work backwards through characters
		if(!isWhitespace(text[i]))
			return text.substr(0, i+1); // Non-whitespace ends here.

	// If got here, everything was whitespace.
	return std::string();
}


const std::string stripHeadAndTailWhitespace(const std::string& text)
{
	return stripHeadWhitespace(stripTailWhitespace(text));
}


const std::string stripWhitespace(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for(size_t i=0; i<s.size(); ++i)
		if(!::isWhitespace(s[i]))
			out.push_back(s[i]);
	return out;
}


const std::string collapseWhitespace(const std::string& s) // Convert runs of 1 or more whitespace characters to just the first whitespace char.
{
	std::string out;
	out.reserve(s.size());
	bool last_char_was_whitespace = false;
	for(size_t i=0; i<s.size(); ++i)
	{
		if(::isWhitespace(s[i]))
		{
			if(!last_char_was_whitespace)
				out.push_back(s[i]);

			last_char_was_whitespace = true;
		}
		else
		{
			out.push_back(s[i]);
			last_char_was_whitespace = false;
		}
	}
	return out;
}


bool isAllWhitespace(const std::string& s)
{
	for(size_t i=0; i<s.size(); ++i)
		if(!::isWhitespace(s[i]))
			return false;
	return true;
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
	std::string lowerstr = text;
	for(size_t i=0; i<lowerstr.size(); ++i)
		if(lowerstr[i] >= 'A' && lowerstr[i] <= 'Z')
			lowerstr[i] = lowerstr[i] - 'A' + 'a';

	return lowerstr;
}


const std::string toUpperCase(const std::string& text)
{
	std::string upperstr = text;
	for(size_t i=0; i<upperstr.size(); ++i)
		if(upperstr[i] >= 'a' && upperstr[i] <= 'z')
			upperstr[i] = upperstr[i] - 'a' + 'A';

	return upperstr;
}


char toLowerCase(char c)
{
	if(c >= 'A' && c <= 'Z')
		return c - 'A' + 'a';
	else
		return c;
}


char toUpperCase(char c)
{
	if(c >= 'a' && c <= 'z')
		return c - 'a' + 'A';
	else
		return c;
}


bool hasExtension(const std::string& file, const std::string& extension)
{
	if(file.length() < extension.length() + 1)
		return false;

	const std::string dot_plus_extension = "." + toUpperCase(extension);

	return toUpperCase(file.substr(file.length() - dot_plus_extension.length(), dot_plus_extension.length())) == dot_plus_extension;
}


const std::string getExtension(const std::string& filename)
{
	const std::string::size_type dot_index = filename.find_last_of(".");

	if(dot_index == std::string::npos)
		return std::string();
	else
		return getTailSubString(filename, dot_index + 1);
}


const std::string eatExtension(const std::string& filename)
{
	return eatSuffix(filename, getExtension(filename));
}


const std::string removeDotAndExtension(const std::string& filename)
{
	return ::eatSuffix(eatExtension(filename), ".");
}


bool hasPrefix(const std::string& s, const string_view& prefix)
{
	if(prefix.length() > s.length())
		return false;

	for(size_t i=0; i<prefix.length(); i++)
		if(prefix[i] != s[i])
			return false;

	return true;
}


bool hasSuffix(const std::string& s, const string_view& suffix)
{
	if(suffix.length() > s.length())
		return false;

	assert(s.length() >= suffix.length());

	const size_t s_offset = s.length() - suffix.length();

	for(size_t i=0; i<suffix.length(); ++i)
		if(s[s_offset + i] != suffix[i])
			return false;

	return true;
}


bool hasLastChar(const std::string& s, char c)
{
	const size_t sz = s.size();
	if(sz == 0)
		return false;
	else
		return s[sz - 1] == c;
}


int getNumMatches(const std::string& s, char target)
{
	int num_matches = 0;

	for(size_t i=0; i<s.length(); i++)
		if(s[i] == target)
			num_matches++;

	return num_matches;
}


// Replaces all occurences of src with dest in string s.
void replaceChar(std::string& s, char src, char dest)
{
	for(size_t i=0; i<s.size(); ++i)
		if(s[i] == src)
			s[i] = dest;
}


// Replaces all occurrences of src with dest in string s.
const std::string StringUtils::replaceCharacter(const std::string& s, char src, char dest)
{
	std::string res = s;
	for(size_t i=0; i<s.size(); ++i)
		if(res[i] == src)
			res[i] = dest;
	return res;
}


const std::string getTailSubString(const std::string& s, size_t first_char_index)
{
	if(first_char_index >= s.size())
		return std::string();

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
		return ::toString(x) + " B";
	else if(x < 1048576)
	{
		const float kbsize = (float)x / 1024.0f;

		return floatToStringNDecimalPlaces(kbsize, 3) + " KB";
	}
	else if(x < 1073741824)
	{
		const float mbsize = (float)x / 1048576.0f;

		return floatToStringNDecimalPlaces(mbsize, 3) + " MB";
	}
	else
	{
		const float gbsize = (float)x / 1073741824.0f;

		return floatToStringNDecimalPlaces(gbsize, 3) + " GB";
	}
}


const std::string getPrefixBeforeDelim(const std::string& s, char delim)
{
	for(unsigned int i=0; i<s.size(); ++i)
		if(s[i] == delim)
			return s.substr(0, i);

	return s;
}


const std::string getSuffixAfterDelim(const std::string& s, char delim)
{
	for(size_t i=0; i<s.size(); ++i)
		if(s[i] == delim)
			return s.substr(i + 1, s.size() - (i + 1));

	return std::string();
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
			parts.push_back(s.substr(start, s.size() - start));
			return parts;
		}

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


const std::string rightSpacePad(const std::string& s, unsigned int minwidth)
{
	return rightPad(s, ' ', minwidth);
}


namespace StringUtils
{


void getPosition(const std::string& str, size_t charindex, size_t& line_num_out, size_t& column_out)
{
//	assert(charindex < str.size());
	if(charindex >= str.size())
	{
		line_num_out = column_out = 0;
		return;
	}

	size_t line = 0;
	size_t col = 0;
	for(size_t i=0; i<charindex; ++i)
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


const std::string getLineFromBuffer(const std::string& str, size_t charindex)
{
//	assert(charindex < str.size());

	std::string line;
	while(charindex < str.size() && str[charindex] != '\n')
	{
		line.push_back(str[charindex]);
		charindex++;
	}

	return line;
}


const std::vector<unsigned char> convertHexToBinary(const std::string& hex)
{
	if(hex.size() % 2 != 0)
		throw StringUtilsExcep("Hex string must have an even number of hex chars.");

	const size_t byte_size = hex.size() / 2; // (hex.size() % 2 == 0) ? hex.size() / 2 : hex.size() / 2 + 1;
	/*std::string res(
		byte_size, // Count
		'\x0' // Char
		);*/
	std::vector<unsigned char> res(byte_size, 0);

	for(size_t i=0; i<hex.size(); i+=2)
	{
		// first 4 bits of byte
		unsigned int x = hexCharToUInt(hex[i]) << 4;

		// Second 4 bits of byte
		if(i + 1 < hex.size())
			x |= hexCharToUInt(hex[i + 1]);

		unsigned char c = (unsigned char)x; 

		res[i/2] = c;// *((char*)&c);
	}
	return res;
}


const std::string convertByteArrayToHexString(const std::vector<unsigned char>& bytes)
{
	return convertByteArrayToHexString(bytes.data(), bytes.size());
}


const std::string convertByteArrayToHexString(const unsigned char* bytes, size_t size)
{
	std::string res;
	res.resize(size * 2);
	for(size_t i=0; i<size; ++i)
	{
		unsigned char b = bytes[i];
		unsigned char upper_nibble = (b & 0xF0u) >> 4;
		unsigned char lower_nibble = b & 0x0Fu;
		if(upper_nibble < 10)
			res[i*2] = '0' + (char)upper_nibble;
		else
			res[i*2] = 'a' + (char)upper_nibble - 10;

		if(lower_nibble < 10)
			res[i*2+1] = '0' + (char)lower_nibble;
		else
			res[i*2+1] = 'a' + (char)lower_nibble - 10;
	}
	return res;
}


const std::vector<unsigned char> stringToByteArray(const std::string& s)
{
	std::vector<unsigned char> bytes(s.size());
	for(size_t i=0; i<s.size(); ++i)
		bytes[i] = s[i];
	return bytes;
}


const std::string byteArrayToString(const std::vector<unsigned char>& bytes)
{
	std::string s;
	s.resize(bytes.size());
	for(size_t i=0; i<bytes.size(); ++i)
		s[i] = bytes[i];
	return s; 
}


#if defined(_WIN32)
const std::wstring UTF8ToWString(const std::string& s)
{
	assert(s.size() < (size_t)std::numeric_limits<int>::max());

	if(s.empty()) // MultiByteToWideChar can't handle zero lengths, handle explicitly.
		return std::wstring();

	// Call initially to get size of buffer to allocate.
	const int size_required = MultiByteToWideChar(
		CP_UTF8, // code page
		0, // flags
		s.data(), // lpMultiByteStr
		(int)s.size(), // cbMultiByte - Size, in bytes, of the string indicated by the lpMultiByteStr parameter.
		NULL, // out buffer
		0 // buffer size
	);

	if(size_required == 0)
		return std::wstring();

	// Call again, this time with the buffer.
	std::wstring res_string;
	res_string.resize(size_required);

	const int result = MultiByteToWideChar(
		CP_UTF8, // code page
		0,
		s.data(),
		(int)s.size(),
		&res_string[0],
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

	return res_string;
}


const std::string WToUTF8String(const std::wstring& wide_string)
{
	assert(wide_string.size() < (size_t)std::numeric_limits<int>::max());

	if(wide_string.empty()) // WideCharToMultiByte can't handle zero lengths, handle explicitly.
		return std::string();

	// Call once to get number of bytes required for buffer.
	const int size_required = WideCharToMultiByte(
		CP_UTF8,
		0,
		wide_string.data(),
		(int)wide_string.size(),
		NULL,
		0,
		NULL,
		NULL
	);

	std::string res_string;
	res_string.resize(size_required);

	/*const int num_bytes = */WideCharToMultiByte(
		CP_UTF8,
		0,
		wide_string.data(),
		(int)wide_string.size(),
		&res_string[0],
		size_required,
		NULL,
		NULL
	);

	return res_string;
}
#endif // defined(_WIN32)


const std::string replaceFirst(const std::string& s, const std::string& target, const std::string& replacement)
{
	const size_t pos = s.find(target);
	if(pos == std::string::npos)
		return s;
	else
	{
		std::string new_string = s;
		new_string.replace(pos, target.length(), replacement);
		return new_string;
	}
}


const std::string replaceAll(const std::string& s, const std::string& target, const std::string& replacement)
{
	assert(!target.empty()); // Target must not be the empty string or the loop below will loop forever.

	size_t searchpos = 0;
	std::string newstring;
	while(1)
	{
		size_t next = s.find(target, searchpos);
		if(next == std::string::npos)
		{
			// No more matches.
			return newstring + s.substr(searchpos, s.size() - searchpos); // Add remaining part of string
		}
		else
		{
			// Found a match at 'next'
			newstring += s.substr(searchpos, next - searchpos); // Add string up to location of target
			newstring += replacement;

			searchpos = next + target.size(); // Advance search pos to one past end of found target.
		}
	}
}


void appendTabbed(std::string& s, const std::string& addition, int num_tabs)
{
	// Add initial tabs
	if(!addition.empty())
		for(int z=0; z<num_tabs; ++z)
			s.push_back('\t');

	for(size_t i=0; i<addition.size(); ++i)
	{
		const char c = addition[i];
		if(c == '\n' && i + 1 < addition.size())
		{
			s.push_back('\n');

			// Add tabs
			for(int z=0; z<num_tabs; ++z)
				s.push_back('\t');
		}
		else
			s.push_back(c);
	}
}


bool containsChar(const std::string& s, char c)
{
	for(size_t i=0; i<s.size(); ++i)
		if(s[i] == c)
			return true;
	return false;
}


bool containsString(const std::string& s, const std::string& target)
{
	return s.find(target) != std::string::npos;
}


// Replace non-printable chars with '?'
const std::string removeNonPrintableChars(const std::string& s)
{
	std::string res = s;
	for(size_t i=0; i<s.size(); ++i)
		if(!isprint(s[i]))
			res[i] = '?';
	return res;
}


const std::vector<std::string> splitIntoLines(const std::string& s)
{
	std::vector<std::string> lines;

	std::string::size_type start = 0; // Char index of start of current line

	const size_t sz = s.size();
	for(size_t i=0; i<sz;)
	{
		if(s[i] == '\r' && (i + 1) < sz && (s[i + 1] == '\n')) // If we encountered a \r\n:
		{
			lines.push_back(s.substr(start, i - start));
			i += 2;
			start = i;
		}
		else if(s[i] == '\n')
		{
			lines.push_back(s.substr(start, i - start));
			i++;
			start = i;
		}
		else
			i++;
	}

	lines.push_back(s.substr(start, sz - start));
	return lines;
}


} // end namespace StringUtils


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"


void StringUtils::test()
{
	conPrint("StringUtils::test()");

	//==================================== Test stripHeadWhitespace ====================================
	testAssert(stripHeadWhitespace("a") == "a");
	testAssert(stripHeadWhitespace("a ") == "a ");
	testAssert(stripHeadWhitespace("a  ") == "a  ");
	testAssert(stripHeadWhitespace(" a") == "a");
	testAssert(stripHeadWhitespace(" ab") == "ab");
	testAssert(stripHeadWhitespace("  ab") == "ab");
	testAssert(stripHeadWhitespace("") == "");
	testAssert(stripHeadWhitespace(" ") == "");
	testAssert(stripHeadWhitespace("  ") == "");

	//==================================== Test stripTailWhitespace ====================================
	testAssert(stripTailWhitespace("a") == "a");
	testAssert(stripTailWhitespace(" a") == " a");
	testAssert(stripTailWhitespace("  a") == "  a");
	testAssert(stripTailWhitespace("a ") == "a");
	testAssert(stripTailWhitespace("a  ") == "a");
	testAssert(stripTailWhitespace("ab  ") == "ab");
	testAssert(stripTailWhitespace("  a  ") == "  a");
	testAssert(stripTailWhitespace("") == "");
	testAssert(stripTailWhitespace(" ") == "");
	testAssert(stripTailWhitespace("  ") == "");

	//==================================== Test stripHeadAndTailWhitespace ====================================
	testAssert(stripHeadAndTailWhitespace("a") == "a");
	testAssert(stripHeadAndTailWhitespace(" a") == "a");
	testAssert(stripHeadAndTailWhitespace("  a") == "a");
	testAssert(stripHeadAndTailWhitespace("a ") == "a");
	testAssert(stripHeadAndTailWhitespace("a  ") == "a");
	testAssert(stripHeadAndTailWhitespace("ab  ") == "ab");
	testAssert(stripHeadAndTailWhitespace("  a  ") == "a");
	testAssert(stripHeadAndTailWhitespace(" a ") == "a");
	testAssert(stripHeadAndTailWhitespace("") == "");
	testAssert(stripHeadAndTailWhitespace(" ") == "");
	testAssert(stripHeadAndTailWhitespace("  ") == "");

	//==================================== Test collapseWhitespace ====================================
	testAssert(collapseWhitespace("a") == "a");
	testAssert(collapseWhitespace(" a") == " a");
	testAssert(collapseWhitespace("  a") == " a");
	testAssert(collapseWhitespace("a ") == "a ");
	testAssert(collapseWhitespace("a  ") == "a ");
	testAssert(collapseWhitespace("ab  ") == "ab ");
	testAssert(collapseWhitespace("  a  ") == " a ");
	testAssert(collapseWhitespace(" a ") == " a ");
	testAssert(collapseWhitespace("") == "");
	testAssert(collapseWhitespace(" ") == " ");
	testAssert(collapseWhitespace("  ") == " ");
	testAssert(collapseWhitespace("a b  c   d    e") == "a b c d e");

	//==================================== Test hasPrefix ====================================
	testAssert(hasPrefix("a", "a"));
	testAssert(!hasPrefix("a", "b"));
	testAssert(hasPrefix("a", ""));
	testAssert(hasPrefix("", ""));
	testAssert(hasPrefix("ab", "a"));
	testAssert(hasPrefix("ab", "ab"));
	testAssert(!hasPrefix("", "a"));
	testAssert(!hasPrefix("a", "ab"));
	testAssert(!hasPrefix("ab", "abc"));

	testAssert(hasPrefix("ab", std::string("a")));


	//==================================== floatToString ====================================
	{
		std::string s = floatToString(123.4567f);
		testAssert(s == "123.4567");
	}
	// Check positive Max representable finite float.
	{
		std::string s = floatToString(std::numeric_limits<float>::max());
		testAssert(s == "3.4028235e38");
	}
	// Check negative Max representable finite float.
	{
		std::string s = floatToString(-std::numeric_limits<float>::max());
		testAssert(s == "-3.4028235e38");
	}

	// Check zero
	{
		std::string s = floatToString(0.f);
		testAssert(s == "0");
	}
	// Check negative (signed) zero.  We want to convert this to "0".
	{
		std::string s = floatToString(-0.f);
		testAssert(s == "0");
	}

	// Check NaN
	{
		std::string s = floatToString(std::numeric_limits<float>::quiet_NaN());
		testAssert(s == "NaN");
	}
	// Check Inf
	{
		std::string s = floatToString(std::numeric_limits<float>::infinity());
		testAssert(s == "Inf");
	}
	// Check -Inf
	{
		std::string s = floatToString(-std::numeric_limits<float>::infinity());
		testAssert(s == "-Inf");
	}


	{
		std::string s;
		for(int i=0; i<100000; ++i)
		{
			float x = (float)i / 1000.0f;
			//float x = uintAsFloat(i);
			s = floatToString(x);

			float x2 = stringToFloat(s);

			testAssert(x == x2);
		}
	}


	// Test float to string
	{
		testAssert(::epsEqual(::stringToFloat(::floatToString(123.456f)), 123.456f));

		testAssert(::floatToStringNDecimalPlaces(123.456f, 0) == "123");
		testAssert(::floatToStringNDecimalPlaces(123.234f, 1) == "123.2");
		testAssert(::floatToStringNDecimalPlaces(123.234f, 2) == "123.23");
		testAssert(::floatToStringNDecimalPlaces(123.234f, 3) == "123.234");
	}

	//================= Test string to float for invalid strings ================================

	try
	{
		stringToFloat("bleh");
		failTest("");
	}
	catch(StringUtilsExcep& )
	{}

	// assert(epsEqual(stringToDouble("-631.3543e52"), -631.3543e52));

	try
	{
		stringToFloat("-z631.3543");
		failTest("");
	}
	catch(StringUtilsExcep& )
	{}

	try
	{
		stringToFloat("");
		failTest("");
	}
	catch(StringUtilsExcep& )
	{}

	try
	{
		stringToFloat(" ");
		failTest("");
	}
	catch(StringUtilsExcep& )
	{}

	//==================================== doubleToString ====================================
	{
		std::string s = doubleToString(123.4567);
		testAssert(s == "123.4567");
	}
	// Check positive Max representable finite float.
	{
		std::string s = doubleToString(std::numeric_limits<double>::max());

		// Google's code returns "1.7976931348623157e308", which differs in the last digit.  Bug in Google's code?

		//testAssert(s == "1.7976931348623158e+308");
	}
	// Check negative Max representable finite float.
	{
		std::string s = doubleToString(-std::numeric_limits<double>::max());
		//testAssert(s == "-1.7976931348623158e+308");
	}

	// Check zero
	{
		std::string s = doubleToString(0.0);
		testAssert(s == "0");
	}
	// Check negative (signed) zero.  We want to convert this to "0".
	{
		std::string s = doubleToString(-0.0);
		testAssert(s == "0");
	}

	// Check NaN
	{
		std::string s = doubleToString(std::numeric_limits<double>::quiet_NaN());
		testAssert(s == "NaN");
	}
	// Check Inf
	{
		std::string s = doubleToString(std::numeric_limits<double>::infinity());
		testAssert(s == "Inf");
	}
	// Check -Inf
	{
		std::string s = doubleToString(-std::numeric_limits<double>::infinity());
		testAssert(s == "-Inf");
	}


	{
		std::string s;
		for(int i=0; i<100000; ++i)
		{
			double x = (float)i / 1000.0;

			s = doubleToString(x);

			double x2 = stringToDouble(s);

			testAssert(x == x2);
		}
	}

	//==================================== Test floatToStringNSigFigs ====================================
	{
		testStringsEqual(floatToStringNSigFigs(std::numeric_limits<float>::quiet_NaN(), 3), "NaN");
		testStringsEqual(floatToStringNSigFigs(std::numeric_limits<float>::infinity(), 3), "Inf");
		testStringsEqual(floatToStringNSigFigs(-std::numeric_limits<float>::infinity(), 3), "-Inf");

		testStringsEqual(floatToStringNSigFigs(1000000.0f, 3), "1000000");
		testStringsEqual(floatToStringNSigFigs(100000.0f, 3), "100000");
		testStringsEqual(floatToStringNSigFigs(10000.0f, 3), "10000");
		testStringsEqual(floatToStringNSigFigs(1000.0f, 3), "1000");
		testStringsEqual(floatToStringNSigFigs(100.0f, 3), "100");
		testStringsEqual(floatToStringNSigFigs(10.0f, 3), "10.0");
		testStringsEqual(floatToStringNSigFigs(1.0f, 3), "1.00");
		testStringsEqual(floatToStringNSigFigs(0.1f, 3), "0.100");
		testStringsEqual(floatToStringNSigFigs(0.01f, 3), "0.0100");
		testStringsEqual(floatToStringNSigFigs(0.001f, 3), "0.00100");
		testStringsEqual(floatToStringNSigFigs(0.0001f, 3), "0.000100");

		testStringsEqual(floatToStringNSigFigs(10.0f, 5), "10.000");
		testStringsEqual(floatToStringNSigFigs(1.0f, 5), "1.0000");

		// Very large/small numbers can be formatted in scientific/exponential mode.
		testStringsEqual(floatToStringNSigFigs(1.0e20f, 3), "1.00e20");
		testStringsEqual(floatToStringNSigFigs(1.0e-20f, 3), "1.00e-20");

		// Test doesn't fail when num sig figs requested is invalid.
		testStringsEqual(floatToStringNSigFigs(1.23f, -1), "1"); // Sig figs gets clamped to 1.

		testStringsEqual(floatToStringNSigFigs(1.0f, 140), "1." + std::string(119, '0')); // Sig figs gets clamped to 120.
		// So result is "1.000000000 ... 000000"  (with 120 - 1 = 119 zeroes)
	}

	//==================================== Test doubleToStringNSigFigs ====================================
	{
		testStringsEqual(doubleToStringNSigFigs(std::numeric_limits<double>::quiet_NaN(), 3), "NaN");
		testStringsEqual(doubleToStringNSigFigs(std::numeric_limits<double>::infinity(), 3), "Inf");
		testStringsEqual(doubleToStringNSigFigs(-std::numeric_limits<double>::infinity(), 3), "-Inf");

		testStringsEqual(doubleToStringNSigFigs(1000000.0, 3), "1000000");
		testStringsEqual(doubleToStringNSigFigs(100000.0, 3), "100000");
		testStringsEqual(doubleToStringNSigFigs(10000.0, 3), "10000");
		testStringsEqual(doubleToStringNSigFigs(1000.0, 3), "1000");
		testStringsEqual(doubleToStringNSigFigs(100.0, 3), "100");
		testStringsEqual(doubleToStringNSigFigs(10.0, 3), "10.0");
		testStringsEqual(doubleToStringNSigFigs(1.0, 3), "1.00");
		testStringsEqual(doubleToStringNSigFigs(0.1, 3), "0.100");
		testStringsEqual(doubleToStringNSigFigs(0.01, 3), "0.0100");
		testStringsEqual(doubleToStringNSigFigs(0.001, 3), "0.00100");
		testStringsEqual(doubleToStringNSigFigs(0.0001, 3), "0.000100");

		testStringsEqual(doubleToStringNSigFigs(10.0, 5), "10.000");
		testStringsEqual(doubleToStringNSigFigs(1.0, 5), "1.0000");

		// Very large/small numbers can be formatted in scientific/exponential mode.
		testStringsEqual(doubleToStringNSigFigs(1.0e20, 3), "1.00e20");
		testStringsEqual(doubleToStringNSigFigs(1.0e-20, 3), "1.00e-20");

		// Test doesn't fail when num sig figs requested is invalid.
		testStringsEqual(doubleToStringNSigFigs(1.23, -1), "1"); // Sig figs gets clamped to 1.

		testStringsEqual(doubleToStringNSigFigs(1.0, 140), "1." + std::string(119, '0')); // Sig figs gets clamped to 120.
		// So result is "1.000000000 ... 000000"  (with 120 - 1 = 119 zeroes)
	}



	//==================================== Test numDigits() ====================================
	for(uint32 i=0; i<10; ++i)
		testAssert(numDigitsUInt32(i) == 1);
	for(uint32 i=10; i<100; ++i)
		testAssert(numDigitsUInt32(i) == 2);
	for(uint32 i=100; i<1000; ++i)
		testAssert(numDigitsUInt32(i) == 3);
	for(uint32 i=1000; i<10000; ++i)
		testAssert(numDigitsUInt32(i) == 4);

	testAssert(numDigitsUInt32(10000u) == 5);
	testAssert(numDigitsUInt32(99999u) == 5);
	testAssert(numDigitsUInt32(100000u) == 6);
	testAssert(numDigitsUInt32(999999u) == 6);
	testAssert(numDigitsUInt32(1000000u) == 7);
	testAssert(numDigitsUInt32(9999999u) == 7);
	testAssert(numDigitsUInt32(10000000u) == 8);
	testAssert(numDigitsUInt32(99999999u) == 8);
	testAssert(numDigitsUInt32(100000000u) == 9);
	testAssert(numDigitsUInt32(999999999u) == 9);
	testAssert(numDigitsUInt32(1000000000u) == 10);
	testAssert(numDigitsUInt32(4294967295u) == 10); // max representable uint32

	testAssert((uint32)4294967295u == std::numeric_limits<uint32>::max());


	for(uint64 i=0; i<10; ++i)
		testAssert(numDigitsUInt64(i) == 1);
	for(uint64 i=10; i<100; ++i)
		testAssert(numDigitsUInt64(i) == 2);
	for(uint64 i=100; i<1000; ++i)
		testAssert(numDigitsUInt64(i) == 3);
	for(uint64 i=1000; i<10000; ++i)
		testAssert(numDigitsUInt64(i) == 4);

	testAssert(numDigitsUInt64(10000ull) == 5);
	testAssert(numDigitsUInt64(99999ull) == 5);
	testAssert(numDigitsUInt64(100000ull) == 6);
	testAssert(numDigitsUInt64(999999ull) == 6);
	testAssert(numDigitsUInt64(1000000ull) == 7);
	testAssert(numDigitsUInt64(9999999ull) == 7);
	testAssert(numDigitsUInt64(10000000ull) == 8);
	testAssert(numDigitsUInt64(99999999ull) == 8);
	testAssert(numDigitsUInt64(100000000ull) == 9);
	testAssert(numDigitsUInt64(999999999ull) == 9);
	testAssert(numDigitsUInt64(1000000000ull) == 10);
	testAssert(numDigitsUInt64(9999999999ull) == 10);
	testAssert(numDigitsUInt64(10000000000ull) == 11);
	testAssert(numDigitsUInt64(99999999999ull) == 11);
	testAssert(numDigitsUInt64(100000000000ull) == 12);
	testAssert(numDigitsUInt64(999999999999ull) == 12);
	testAssert(numDigitsUInt64(1000000000000ull) == 13);
	testAssert(numDigitsUInt64(9999999999999ull) == 13);
	testAssert(numDigitsUInt64(10000000000000ull) == 14);
	testAssert(numDigitsUInt64(99999999999999ull) == 14);
	testAssert(numDigitsUInt64(100000000000000ull) == 15);
	testAssert(numDigitsUInt64(999999999999999ull) == 15);
	testAssert(numDigitsUInt64(1000000000000000ull) == 16);
	testAssert(numDigitsUInt64(9999999999999999ull) == 16);
	testAssert(numDigitsUInt64(10000000000000000ull) == 17);
	testAssert(numDigitsUInt64(99999999999999999ull) == 17);
	testAssert(numDigitsUInt64(100000000000000000ull) == 18);
	testAssert(numDigitsUInt64(999999999999999999ull) == 18);
	testAssert(numDigitsUInt64(1000000000000000000ull) == 19);
	testAssert(numDigitsUInt64(9999999999999999999ull) == 19);
	testAssert(numDigitsUInt64(10000000000000000000ull) == 20);
	testAssert(numDigitsUInt64(18446744073709551615ull) == 20); // max representable uint64

	testAssert((uint64)18446744073709551615ULL == std::numeric_limits<uint64>::max());

	//=========================== Test uInt32ToString() ====================================
	
	testAssert(uInt32ToString(1234) == "1234");
	testAssert(uInt32ToString(0) == "0");
	testAssert(uInt32ToString(1) == "1");
	testAssert(uInt32ToString(9) == "9");
	testAssert(uInt32ToString(10) == "10");
	testAssert(uInt32ToString(11) == "11");
	testAssert(uInt32ToString(99) == "99");
	testAssert(uInt32ToString(100) == "100");
	testAssert(uInt32ToString(101) == "101");
	testAssert(uInt32ToString(999) == "999");
	testAssert(uInt32ToString(1000) == "1000");
	testAssert(uInt32ToString(1001) == "1001");
	testAssert(uInt32ToString(9999) == "9999");
	testAssert(uInt32ToString(10000) == "10000");
	testAssert(uInt32ToString(10001) == "10001");
	testAssert(uInt32ToString(99999) == "99999");
	testAssert(uInt32ToString(100000) == "100000");
	testAssert(uInt32ToString(100001) == "100001");
	testAssert(uInt32ToString(999999) == "999999");

	testAssert(uInt32ToString(1234567) == "1234567");

	testAssert(uInt32ToString(1000000) == "1000000");
	testAssert(uInt32ToString(1000001) == "1000001");
	testAssert(uInt32ToString(9999999) == "9999999");
	testAssert(uInt32ToString(10000000) == "10000000");
	testAssert(uInt32ToString(10000001) == "10000001");
	testAssert(uInt32ToString(99999999) == "99999999");
	testAssert(uInt32ToString(100000000) == "100000000");
	testAssert(uInt32ToString(100000001) == "100000001");
	testAssert(uInt32ToString(999999999) == "999999999");

	testAssert(uInt32ToString(1000000000) == "1000000000");
	testAssert(uInt32ToString(1000000001) == "1000000001");

	testAssert(uInt32ToString(4294967294u) == "4294967294");
	testAssert(uInt32ToString(4294967295u) == "4294967295"); // max representable uint32


	//=========================== Test uInt64ToString() ====================================
	
	testAssert(uInt64ToString(1234LL) == "1234");
	testAssert(uInt64ToString(12345671234567LL) == "12345671234567");
	testAssert(uInt64ToString(0) == "0");
	testAssert(uInt64ToString(1) == "1");
	testAssert(uInt64ToString(9) == "9");
	testAssert(uInt64ToString(10) == "10");
	testAssert(uInt64ToString(11) == "11");
	testAssert(uInt64ToString(99) == "99");
	testAssert(uInt64ToString(100) == "100");
	testAssert(uInt64ToString(101) == "101");
	testAssert(uInt64ToString(999) == "999");
	testAssert(uInt64ToString(1000) == "1000");
	testAssert(uInt64ToString(1001) == "1001");
	testAssert(uInt64ToString(9999) == "9999");
	testAssert(uInt64ToString(10000) == "10000");
	testAssert(uInt64ToString(10001) == "10001");
	testAssert(uInt64ToString(99999) == "99999");
	testAssert(uInt64ToString(100000) == "100000");
	testAssert(uInt64ToString(100001) == "100001");
	testAssert(uInt64ToString(999999) == "999999");
	testAssert(uInt64ToString(1234567) == "1234567");
	testAssert(uInt64ToString(100000000000ULL) == "100000000000");
	testAssert(uInt64ToString(100000000001ULL) == "100000000001");
	testAssert(uInt64ToString(999999999999ULL) == "999999999999");
	testAssert(uInt64ToString(10000000000000000ULL) == "10000000000000000");
	testAssert(uInt64ToString(10000000000000001ULL) == "10000000000000001");
	testAssert(uInt64ToString(99999999999999999ULL) == "99999999999999999");
	testAssert(uInt64ToString(18446744073709551614ULL) == "18446744073709551614");
	testAssert(uInt64ToString(18446744073709551615ULL) == "18446744073709551615"); // max representable uint64
	
	/*
	Performance tests

	On Nick's i7 3770:
	
	referenceInt32ToString() time: 154.28920611157082 ns
	int32ToString() time: 45.05886929109693 ns
	oldUInt64ToString() time: 158.7496156571433 ns
	uInt64ToString() time: 43.24579276726581 ns
	per numDigitsUInt64 time: 2.343145606573671 ns
	randomised per numDigitsUInt64 time: 4.963372484780848 ns
	*/
	if(false)
	{
		{
			const int N = 100000;
			Timer timer;
			int sum = 0;
			for(int i=0; i<N; ++i)
			{
				const std::string s = referenceInt32ToString(i);
				sum += (int)s[0];
			}

			double elapsed = timer.elapsed();
			printVar(sum);
			conPrint("referenceInt32ToString() time: " + toString(1.0e9 * elapsed / N) + " ns");
		}

		{
			const int N = 100000;
			Timer timer;
			int sum = 0;
			for(int i=0; i<N; ++i)
			{
				const std::string s = int32ToString(i);
				sum += (int)s[0];
			}

			double elapsed = timer.elapsed();
			printVar(sum);
			conPrint("int32ToString() time: " + toString(1.0e9 * elapsed / N) + " ns");
		}

		/*{
			const int N = 100000;
			Timer timer;
			int sum = 0;
			for(int i=0; i<N; ++i)
			{
				const std::string s = oldUInt64ToString((uint64)i);
				sum += (int)s[0];
			}

			double elapsed = timer.elapsed();
			printVar(sum);
			conPrint("oldUInt64ToString() time: " + toString(1.0e9 * elapsed / N) + " ns");
		}*/

		{
			const int N = 100000;
			Timer timer;
			int sum = 0;
			for(int i=0; i<N; ++i)
			{
				const std::string s = uInt64ToString((uint64)i);
				sum += (int)s[0];
			}

			double elapsed = timer.elapsed();
			printVar(sum);
			conPrint("uInt64ToString() time: " + toString(1.0e9 * elapsed / N) + " ns");
		}

		{
			const int N = 100000;
			Timer timer;
			int sum = 0;
			for(int i=0; i<N; ++i)
			{
				sum += numDigitsUInt64(i);
			}

			double elapsed = timer.elapsed();
			printVar(sum);
			conPrint("per numDigitsUInt64 time: " + toString(1.0e9 * elapsed / N) + " ns");
		}

		/*{
			const int N = 100000;
			PCG32 rng(1);
			std::vector<int> v(N);
			for(int i=0; i<N; ++i)
				v[i] = (int)(rng.unitRandom() * N);

			Timer timer;
			int sum = 0;
			for(int i=0; i<N; ++i)
			{
				sum += numDigitsUInt64(v[i]);
			}

			double elapsed = timer.elapsed();
			printVar(sum);
			conPrint("randomised per numDigitsUInt64 time: " + toString(1.0e9 * elapsed / N) + " ns");
		}*/
	}


	//===================================== int32ToString() ============================================
	
	// Test numbers near to int32 min.
	for(int32 i=std::numeric_limits<int32>::min(); i != std::numeric_limits<int32>::min() + 1000; ++i)
		testAssert(int32ToString(i) == referenceInt32ToString(i));

	// Test numbers near to int32 max
	for(int32 i=std::numeric_limits<int32>::max() - 1000; i != std::numeric_limits<int32>::max(); ++i)
		testAssert(int32ToString(i) == referenceInt32ToString(i));

	// Test numbers around zero.
	for(int32 i=-1000; i != 1000; ++i)
		testAssert(int32ToString(i) == referenceInt32ToString(i));


	testAssert(int32ToString(1234567) == "1234567");
	testAssert(int32ToString(-1234567) == "-1234567");
	testAssert(int32ToString(0) == "0");


	//===================================== int64ToString() ============================================
	
	testAssert(int64ToString(-1) == "-1");
	testAssert(int64ToString(0) == "0");
	testAssert(int64ToString(1) == "1");
	testAssert(int64ToString(1234567) == "1234567");
	testAssert(int64ToString(-1234567) == "-1234567");
	testAssert(int64ToString(-1234567) == "-1234567");

	testAssert(std::numeric_limits<int64>::min() == -9223372036854775807LL - 1LL);
	testAssert(int64ToString(-9223372036854775807LL - 1LL) == "-9223372036854775808");

	testAssert(std::numeric_limits<int64>::max() == 9223372036854775807LL);
	testAssert(int64ToString(9223372036854775807LL) == "9223372036854775807");




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


	//===================================== test replaceFirst() =======================================
	// const std::string replaceFirst(const std::string& s, const std::string& target, const std::string& replacement);
	testAssert(replaceFirst("", "", "01") == "01");
	testAssert(replaceFirst("abcd", "", "01") == "01abcd");

	// Test single occurrence of target.
	testAssert(replaceFirst("abcd", "ab", "01") == "01cd");
	
	// Test multiple occurrences of target.
	testAssert(replaceFirst("abab", "ab", "01") == "01ab");

	// Test long replacement string
	testAssert(replaceFirst("abcd", "ab", "0123456") == "0123456cd");
	testAssert(replaceFirst("abcd", "cd", "0123456") == "ab0123456");

	// Test no match
	testAssert(replaceFirst("", "ef", "0123456") == "");
	testAssert(replaceFirst("abcd", "ef", "0123456") == "abcd");
	testAssert(replaceFirst("abcd", "abcde", "0123456") == "abcd");

	//===================================== test replaceAll() =======================================
	// const std::string replaceAll(const std::string& s, const std::string& target, const std::string& replacement);

	// Test single occurrence of target.
	testAssert(replaceAll("abcd", "ab", "01") == "01cd");
	
	// Test multiple occurrences of target.
	testAssert(replaceAll("abab", "ab", "01") == "0101");

	testAssert(replaceAll("abcdab", "ab", "01") == "01cd01");

	// Test long replacement string
	testAssert(replaceAll("abcd", "ab", "0123456") == "0123456cd");
	testAssert(replaceAll("abcd", "cd", "0123456") == "ab0123456");

	// Test no match
	testAssert(replaceAll("", "ef", "0123456") == "");
	testAssert(replaceAll("abcd", "ef", "0123456") == "abcd");
	testAssert(replaceAll("abcd", "abcde", "0123456") == "abcd");

	//===================================== test floatLiteralString() ==================================
	testAssert(floatLiteralString(1.2f) == "1.2f");
	testAssert(floatLiteralString(1.f) == "1.f");
	testAssert(floatLiteralString(2.0f) == "2.f");
	testAssert(floatLiteralString(std::numeric_limits<float>::infinity()) == "Inf");
	testAssert(floatLiteralString(-std::numeric_limits<float>::infinity()) == "-Inf");
	testAssert(floatLiteralString(std::numeric_limits<float>::quiet_NaN()) == "NaN");

	testAssert(floatLiteralString(1.0e-30f) == "1.0e-30f" || floatLiteralString(1.0e-30f) == "1.e-30f" || floatLiteralString(1.0e-30f) == "1e-30f");

	//===================================== test doubleLiteralString() ==================================
	testAssert(doubleLiteralString(1.2) == "1.2");
	testAssert(doubleLiteralString(1.) == "1.");
	testAssert(doubleLiteralString(2.0) == "2.");
	testAssert(doubleLiteralString(std::numeric_limits<double>::infinity()) == "Inf");
	testAssert(doubleLiteralString(-std::numeric_limits<double>::infinity()) == "-Inf");
	testAssert(doubleLiteralString(std::numeric_limits<double>::quiet_NaN()) == "NaN");

	{
		const std::string s = doubleLiteralString(1.0e-30);
		testAssert(s == "1.0e-30" || s == "1.e-30" || s == "1e-30");
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


	// Test WToUTF8String and UTF8ToWString.
#if defined(_WIN32)
	{
		// const int a = sizeof(wchar_t);

		const std::wstring w = L"A";

		const std::string s = StringUtils::WToUTF8String(w);

		testAssert(s.size() == 1);
		testAssert(s.c_str()[0] == 'A');
		testAssert(s.c_str()[1] == '\0');

		const std::wstring w2 = StringUtils::UTF8ToWString(s);

		testAssert(w == w2);
	}

	// Test with empty string
	{
		const std::wstring w = L"";

		const std::string s = StringUtils::WToUTF8String(w);

		testAssert(s.size() == 0);

		const std::wstring w2 = StringUtils::UTF8ToWString(s);

		testAssert(w == w2);
	}


	{
		// This is the Euro sign, encoded in UTF-8: see http://en.wikipedia.org/wiki/UTF-8
		const std::string s = "\xE2\x82\xAC";

		testAssert(s.size() == 3);

		const std::wstring w = StringUtils::UTF8ToWString(s);

		const std::string s2 = StringUtils::WToUTF8String(w);

		testAssert(s == s2);
	}
#endif

	//===================================== test split() ==================================
	std::vector<std::string> parts;

	parts = split("", ':');
	testAssert(parts.size() == 1 && parts[0] == "");
	testAssert(StringUtils::join(parts, ":") == "");

	parts = split(":", ':');
	testAssert(parts.size() == 2 && parts[0] == "" && parts[1] == "");
	testAssert(StringUtils::join(parts, ":") == ":");

	parts = split("::", ':');
	testAssert(parts.size() == 3 && parts[0] == "" && parts[1] == "" && parts[2] == "");
	testAssert(StringUtils::join(parts, ":") == "::");

	
	parts = split("a::b", ':');
	testAssert(parts.size() == 3 && parts[0] == "a" && parts[1] == "" && parts[2] == "b");
	testAssert(StringUtils::join(parts, ":") == "a::b");

	parts = split("a:b:c", ':');
	testAssert(parts.size() == 3);
	testAssert(parts[0] == "a");
	testAssert(parts[1] == "b");
	testAssert(parts[2] == "c");


	parts = split("a:b:", ':');
	testAssert(parts.size() == 3);
	testAssert(parts[0] == "a");
	testAssert(parts[1] == "b");
	testAssert(parts[2] == "");

	parts = split(":a:b:", ':');
	testAssert(parts.size() == 4);
	testAssert(parts[0] == "");
	testAssert(parts[1] == "a");
	testAssert(parts[2] == "b");
	testAssert(parts[3] == "");

	parts = split(":a:b", ':');
	testAssert(parts.size() == 3);
	testAssert(parts[0] == "");
	testAssert(parts[1] == "a");
	testAssert(parts[2] == "b");
	

	// Test rightPad()
	testAssert(rightPad("123", 'a', 5) == "123aa");
	testAssert(rightPad("12345", 'a', 3) == "12345");

	// Test join()
	parts = split("a:b:c", ':');
	testAssert(StringUtils::join(parts, ":") == "a:b:c");

	parts = split("a", ':');
	testAssert(StringUtils::join(parts, ":") == "a");

	parts = split("", ':');
	testAssert(StringUtils::join(parts, ":") == "");
	

	//===================================== test splitIntoLines() ==================================
	{
		parts = splitIntoLines("");
		testAssert(parts.size() == 1 && parts[0] == "");
		testAssert(StringUtils::join(parts, ":") == "");

		parts = splitIntoLines("\n");
		testAssert(parts.size() == 2 && parts[0] == "" && parts[1] == "");

		parts = splitIntoLines("\n\n");
		testAssert(parts.size() == 3 && parts[0] == "" && parts[1] == "" && parts[2] == "");


		parts = splitIntoLines("a\n\nb");
		testAssert(parts.size() == 3 && parts[0] == "a" && parts[1] == "" && parts[2] == "b");

		parts = splitIntoLines("a\nb\nc");
		testAssert(parts.size() == 3);
		testAssert(parts[0] == "a");
		testAssert(parts[1] == "b");
		testAssert(parts[2] == "c");

		parts = splitIntoLines("a\nb\n");
		testAssert(parts.size() == 3);
		testAssert(parts[0] == "a");
		testAssert(parts[1] == "b");
		testAssert(parts[2] == "");

		parts = splitIntoLines("\na\nb\n");
		testAssert(parts.size() == 4);
		testAssert(parts[0] == "");
		testAssert(parts[1] == "a");
		testAssert(parts[2] == "b");
		testAssert(parts[3] == "");

		parts = splitIntoLines("\na\nb");
		testAssert(parts.size() == 3);
		testAssert(parts[0] == "");
		testAssert(parts[1] == "a");
		testAssert(parts[2] == "b");

		// Test with \r also
		parts = splitIntoLines("a\r\nb\nc");
		testAssert(parts.size() == 3);
		testAssert(parts[0] == "a");
		testAssert(parts[1] == "b");
		testAssert(parts[2] == "c");

		parts = splitIntoLines("\r\n");
		testAssert(parts.size() == 2 && parts[0] == "" && parts[1] == "");

		parts = splitIntoLines("\r");
		testAssert(parts.size() == 1 && parts[0] == "\r");
	}


	

	/*std::string current_locale = std::setlocale(LC_ALL, NULL);

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
		const char* result = std::setlocale(LC_ALL, current_locale.c_str());
		testAssert(result != NULL);
	}*/


	

	testAssert(epsEqual((double)stringToInt("-631"), (double)-631));


	try
	{
		stringToInt("dfgg");
		testAssert(false);
	}
	catch(StringUtilsExcep& )
	{}


	{
		const std::string s = "\n\
			line1\n\
			line2\n\
			line3";

		size_t line, col;
		StringUtils::getPosition(s, 4, line, col);
		testAssert(line == 1 && col == 3);
	}



	testAssert(::toUpperCase("meh666XYZ") == "MEH666XYZ");

	testAssert(::toLowerCase("meh666XYZ") == "meh666xyz");

	testAssert(::toLowerCase('C') == 'c');
	testAssert(::toLowerCase('1') == '1');
	testAssert(::toLowerCase('c') == 'c');

	testAssert(::toUpperCase('C') == 'C');
	testAssert(::toUpperCase('1') == '1');
	testAssert(::toUpperCase('c') == 'C');

	testAssert(getPrefixBeforeDelim("meh666", '6') == "meh");
	testAssert(getPrefixBeforeDelim("meh666", '3') == "meh666");
	testAssert(getPrefixBeforeDelim("meh666", 'm') == "");

	testAssert(getSuffixAfterDelim("a_b", '_') == "b");
	testAssert(getSuffixAfterDelim("a_bc", '_') == "bc");
	testAssert(getSuffixAfterDelim("a_", '_') == "");
	testAssert(getSuffixAfterDelim("a", '_') == "");
	testAssert(getSuffixAfterDelim("", '_') == "");
	testAssert(getSuffixAfterDelim("_b", '_') == "b");
	testAssert(getSuffixAfterDelim("_", '_') == "");
	testAssert(getSuffixAfterDelim("_bc", '_') == "bc");
	testAssert(getSuffixAfterDelim("_bc_de", '_') == "bc_de");
	testAssert(getSuffixAfterDelim("__de", '_') == "_de");
	testAssert(getSuffixAfterDelim("abc_", '_') == "");

	testAssert(::toHexString(0x0) == "0");
	testAssert(::toHexString(0x00000) == "0");
	testAssert(::toHexString(0x1) == "1");
	testAssert(::toHexString(0x00000001) == "1");
	testAssert(::toHexString(0x2) == "2");
	testAssert(::toHexString(0x3) == "3");
	testAssert(::toHexString(0x4) == "4");
	testAssert(::toHexString(0x8) == "8");
	testAssert(::toHexString(0xF) == "F");
	testAssert(::toHexString(0x10) == "10");
	testAssert(::toHexString(0x11) == "11");
	testAssert(::toHexString(0xA4) == "A4");
	testAssert(::toHexString(0xFF) == "FF");
	testAssert(::toHexString(0x34bc8106) == "34BC8106");
	testAssert(::toHexString(0xA4) == "A4");
	testAssert(::toHexString(0xFF) == "FF");
	testAssert(::toHexString(0xFFFFFFFF) == "FFFFFFFF");
	testAssert(::toHexString(0xFFFFFFFFFFFFFFFFull) == "FFFFFFFFFFFFFFFF");
	testAssert(::toHexString(0xFF00000000000000ull) == "FF00000000000000");
	testAssert(::toHexString(0x123456789ABCDEF0ull) == "123456789ABCDEF0");
	testAssert(::toHexString(0xFFFFFFFFFFFFFFFull) == "FFFFFFFFFFFFFFF");

	//=========================== toHexString perf tests ========================
	if(false)
	{
		{
			const int N = 100000;
			Timer timer;
			int sum = 0;
			for(int i=0; i<N; ++i)
			{
				const std::string s = toHexString((uint64)i);
				sum += (int)s[0];
			}

			double elapsed = timer.elapsed();
			printVar(sum);
			conPrint("toHexString() time: " + toString(1.0e9 * elapsed / N) + " ns");
		}
	}


	//======================== hexStringToUInt32() ==========================
	testAssert(::hexStringToUInt32("34bc8106") == 0x34bc8106);
	testAssert(::hexStringToUInt32("0005F") == 0x0005F);
	testAssert(::hexStringToUInt32("0") == 0x0);
	testAssert(::hexStringToUInt32("FFFFFFFF") == 0xFFFFFFFF);
	try
	{
		hexStringToUInt32("Z");
		failTest("Should have thrown exception.");
	}
	catch(StringUtilsExcep&)
	{
	}
	try
	{
		hexStringToUInt32("FFFFFFFFF"); // too long
		failTest("Should have thrown exception.");
	}
	catch(StringUtilsExcep&)
	{
	}


	//======================== hexStringToUInt64() ==========================
	testAssert(::hexStringToUInt64("34bc81062bcD23") == 0x34bc81062bcD23ULL);
	testAssert(::hexStringToUInt64("0005F") == 0x0005F);
	testAssert(::hexStringToUInt64("0") == 0x0);
	testAssert(::hexStringToUInt64("FFFFFFFFFFFFFFFF") == 0xFFFFFFFFFFFFFFFFULL);
	try
	{
		hexStringToUInt64("Z");
		failTest("Should have thrown exception.");
	}
	catch(StringUtilsExcep&)
	{
	}
	try
	{
		hexStringToUInt64("FFFFFFFFFFFFFFFFF"); // too long
		failTest("Should have thrown exception.");
	}
	catch(StringUtilsExcep&)
	{
	}


	//======================== intToHexChar() ==========================
	testAssert(intToHexChar(0) == '0');
	testAssert(intToHexChar(1) == '1');
	testAssert(intToHexChar(8) == '8');
	testAssert(intToHexChar(9) == '9');
	testAssert(intToHexChar(10) == 'A');
	testAssert(intToHexChar(11) == 'B');
	testAssert(intToHexChar(12) == 'C');
	testAssert(intToHexChar(13) == 'D');
	testAssert(intToHexChar(14) == 'E');
	testAssert(intToHexChar(15) == 'F');
	try
	{
		intToHexChar(-1);
		failTest("Should have thrown exception.");
	}
	catch(StringUtilsExcep&)
	{}
	try
	{
		intToHexChar(16);
		failTest("Should have thrown exception.");
	}
	catch(StringUtilsExcep&)
	{
	}

	testAssert(::charToString('a') == std::string("a"));
	testAssert(::charToString(' ') == std::string(" "));

	testAssert(::hasSuffix("test", "st"));
	testAssert(::hasSuffix("test", "t"));
	testAssert(::hasSuffix("test", "est"));
	testAssert(::hasSuffix("test", "test"));
	testAssert(::hasSuffix("test", ""));
	testAssert(!::hasSuffix("test", "a"));
	testAssert(!::hasSuffix("test", "atest"));
	testAssert(!::hasSuffix("test", "aest"));
	testAssert(!::hasSuffix("test", "ast"));

	testAssert(::hasLastChar("t", 't'));
	testAssert(!::hasLastChar("t", 'a'));
	testAssert(::hasLastChar("test", 't'));
	testAssert(!::hasLastChar("test", 'a'));
	testAssert(!::hasLastChar("", 't'));
	testAssert(!::hasLastChar("", '\0'));

	testAssert(::isWhitespace(' '));
	testAssert(::isWhitespace('\t'));
	testAssert(::isWhitespace('	'));

	//======================== intToHexChar() ==========================

	testAssert(getTailSubString("abc", 0) == "abc");
	testAssert(getTailSubString("abc", 1) == "bc");
	testAssert(getTailSubString("abc", 2) == "c");
	testAssert(getTailSubString("abc", 3) == "");
	testAssert(getTailSubString("abc", 4) == "");

/*	testAssert(StringUtils::convertHexToBinary("AB") == "\xAB");

	testAssert(StringUtils::convertHexToBinary("02") == "\x02");
	testAssert(StringUtils::convertHexToBinary("5f") == "\x5f");
	testAssert(StringUtils::convertHexToBinary("12") == "\x12");

	testAssert(StringUtils::convertHexToBinary("12AB") == "\x12\xAB");*/

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

	

	const int testint = 42;
	assert(writeToStringStream(testint) == "42");
	assert(writeToStringStream("bla") == "bla");

	const std::string teststring = "42";
	int resultint;
	readFromStringStream(teststring, resultint);
	assert(resultint == 42);

*/

	conPrint("StringUtils::test() done");
}


#endif // BUILD_TESTS
