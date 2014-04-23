/*=====================================================================
Parser.cpp
----------
Copyright Glare Technologies Limited 2013 - 
=====================================================================*/
#include "Parser.h"


#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../maths/mathstypes.h"
#include "../double-conversion/double-conversion.h"
#include <limits>


Parser::Parser(const char* text_, unsigned int textsize_)
:	text(text_),
	textsize(textsize_)
{
	currentpos = 0;

	// Get the current decimal point character from the locale info, may be '.' or ','
	/*struct lconv* lc = std::localeconv();

	if(std::strlen(lc->decimal_point) >= 1)
		this->decimal_separator = lc->decimal_point[0];
	else*/
		this->decimal_separator = '.';
}


Parser::Parser()
{
	this->text = NULL;
	this->textsize = 0;
	this->currentpos = 0;

	// Get the current decimal point character from the locale info, may be '.' or ','
	/*struct lconv* lc = std::localeconv();

	if(std::strlen(lc->decimal_point) >= 1)
		this->decimal_separator = lc->decimal_point[0];
	else*/
		this->decimal_separator = '.';
}


Parser::~Parser()
{
}


void Parser::reset(const char* text_, unsigned int textsize_)
{
	this->text = text_;
	this->textsize = textsize_;
	this->currentpos = 0;
	// Leave this->decimal_separator as it is.
}


static const unsigned int ASCII_ZERO_UINT = (unsigned int)'0';


/*
will consider overflowed if x > 2147483648

Suppose x = 214748364
x * 10 = 2147483640
suppose x = 214748365
x * 10 = 2147483650   [Overflow]


Let us consider our result overflowed if > 2147483648

say we have x >= 0 && x <= 2147483648
and we are adding an integer 'd' such that d >= 0 && d <= 9.
Result x + d will overflow iff
x + d > 2147483648
=> d > 2147483648 - x
*/
bool Parser::parseInt(int32& result_out)
{
	unsigned int pos = currentpos;

	/// Parse sign ///
	int sign = 1;
	if(pos >= textsize) // if EOF
		return false;
	if(text[pos] == '+')
		pos++;
	else if(text[pos] == '-')
	{
		pos++;
		sign = -1;
	}

	/// Parse digits ///
	const unsigned int digit_start_pos = pos;
	unsigned int x = 0;
	for( ; (pos < textsize) && ::isNumeric(text[pos]); ++pos)
	{
		if(x > 214748364u) // If x will overflow when multiplied by 10:
			return false;

		// At this point we know:
		assert(x <= 214748364u);
		assert(10*x <= 2147483640u);
		
		x *= 10;

		assert(x <= 2147483640u);

		const unsigned int d = (unsigned int)text[pos] - ASCII_ZERO_UINT;
		assert(d >= 0 && d <= 9);

		x += d;

		assert(x <= 2147483649u);
	}

	// If there were no digits, then this is not a valid integer literal
	if(pos == digit_start_pos)
		return false;

	assert(x <= 2147483649u);
	if(x >= 2147483648u)
	{
		// Possible overflow
		if(x == 2147483648u)
		{
			if(sign == 1) // 2147483648 is not representable as a positive signed integer.
				return false;
			else
			{			
				assert(sign == -1);
				// else if sign = -1, then result = -2147483648, which is valid.
				// We can't cast x to int though, as it will overflow.  So just return the result directly.
				// -2147483648 can't be written directly as 2147483648 is too large for a signed integer.
				result_out = -2147483647 - 1;
				this->currentpos = pos;
				return true;
			}
		}
		else
		{
			// x > 2147483648u, overflow.
			assert(x > 2147483648u);
			return false;
		}
	}

	assert(x <= 2147483647u);

	result_out = sign * (int)x;
	this->currentpos = pos;
	return true;
}


bool Parser::parseUnsignedInt(uint32& result_out)
{
	// Use 64 bit var
	uint64 x = 0;
	
	unsigned int pos = this->currentpos;
	const unsigned int initial_pos = this->currentpos;
	
	for( ;pos < textsize && ::isNumeric(text[pos]); ++pos)
	{
		x = 10*x + ((uint64)text[pos] - (uint64)'0');

		// Check if the value is too large to hold in a 32 bit uint:
		if(x > 4294967295u)
			return false;
	}

	result_out = (uint32)x;
	this->currentpos = pos;

	return pos != initial_pos;

	/*unsigned int x = 0;
	const int initial_currentpos = currentpos;
	for( ;notEOF() && ::isNumeric(text[currentpos]); ++currentpos)
		x = 10*x + ((int)text[currentpos] - (int)'0');
	result_out = x;
	return currentpos - initial_currentpos > 0;*/
}


/*bool Parser::parseNDigitUnsignedInt(unsigned int N, uint32& result_out)
{
	// Use 64 bit var
	uint64 x = 0;
	const int initial_currentpos = currentpos;
	for( ;notEOF() && ::isNumeric(text[currentpos]) && (currentpos - initial_currentpos < N); ++currentpos)
	{
		x = 10*x + ((uint64)text[currentpos] - (uint64)'0');

		// Check if the value is too large to hold in a 32 bit uint:
		if(x > 4294967295u)
		{
			currentpos = initial_currentpos; // Restore currentpos.
			return false;
		}
	}
	result_out = (uint32)x;
	return currentpos - initial_currentpos == N;

	//unsigned int x = 0;
	//const int initial_currentpos = currentpos;
	//for( ;notEOF() && ::isNumeric(text[currentpos]) && (currentpos - initial_currentpos < N); ++currentpos)
	//	x = 10*x + ((int)text[currentpos] - (int)'0');
	//result_out = x;
	//return currentpos - initial_currentpos == N;
}*/


bool Parser::parseFloat(float& result_out)
{
	if(eof())
		return false;

	double_conversion::StringToDoubleConverter s2d_converter(
		double_conversion::StringToDoubleConverter::ALLOW_TRAILING_JUNK,
		std::numeric_limits<float>::quiet_NaN(), // empty string value
		std::numeric_limits<float>::quiet_NaN(), // junk string value.  We'll use this to detect failed parses.
		NULL, // infinity symbol
		NULL // NaN symbol
	);

	const int remaining_len = textsize - currentpos;

	int num_processed_chars = 0;
	const float x = s2d_converter.StringToFloat(&text[currentpos], remaining_len, &num_processed_chars);
	if(::isNAN(x))
		return false; // Parsing failed

	result_out = x;
	currentpos += num_processed_chars;

	// Parse optional 'f' or 'F' (single-precision floating point number specifier)
	if(notEOF() && current() == 'f')
		parseChar('f');
	else if(notEOF() && current() == 'F')
		parseChar('F');
	
	return true;
}


bool Parser::fractionalNumberNext()
{
	//TODO FIXME NOTE: this is incorrect.  Returns false on 4e-5f.

	const unsigned int initial_currentpos = currentpos;

	if(eof())
		return false;

	if(parseChar('+'))
	{}
	else if(parseChar('-'))
	{}

	if(eof())
	{
		currentpos = initial_currentpos; // restore currentpos
		return false;
	}

	bool valid_number = false;

	while(notEOF() && isNumeric(current()))
	{
		valid_number = true;
		currentpos++;
	}

	if(!valid_number)
	{
		currentpos = initial_currentpos; // restore currentpos
		return false;
	}

	if(parseChar('.'))
	{
		currentpos = initial_currentpos; // restore currentpos
		return true;
	}
	else
	{
		if(currentIsChar('e') || currentIsChar('E'))
		{
			// TODO: check that [sign]digits follows?

			currentpos = initial_currentpos; // restore currentpos
			return true;
		}

		currentpos = initial_currentpos; // restore currentpos
		return false;
	}
}


//[whitespace] [sign] [digits] [.digits] [ {d | D | e | E }[sign]digits]

//must be whitespace delimited
bool Parser::parseDouble(double& result_out)
{
	if(eof())
		return false;

	double_conversion::StringToDoubleConverter s2d_converter(
		double_conversion::StringToDoubleConverter::ALLOW_TRAILING_JUNK,
		std::numeric_limits<double>::quiet_NaN(), // empty string value
		std::numeric_limits<double>::quiet_NaN(), // junk string value.  We'll use this to detect failed parses.
		NULL, // infinity symbol
		NULL // NaN symbol
	);

	const int remaining_len = textsize - currentpos;

	int num_processed_chars = 0;
	const double x = s2d_converter.StringToDouble(&text[currentpos], remaining_len, &num_processed_chars);
	if(::isNAN(x))
		return false; // Parsing failed

	result_out = x;
	currentpos += num_processed_chars;

	// Parse optional 'f' or 'F' (single-precision floating point number specifier)
	if(notEOF() && current() == 'f')
		parseChar('f');
	else if(notEOF() && current() == 'F')
		parseChar('F');
	
	return true;


	/*const unsigned int initial_currentpos = currentpos;

	if(eof())
		return false;
	
	/// Parse sign ///
	double sign = 1.;
	
	if(parseChar('+'))
	{}
	else if(parseChar('-'))
		sign = -1.;

	if(eof())// || !isNumeric(current()))
	{
		currentpos = initial_currentpos;
		return false;
	}

	bool reached_numerals = false;

	/// Parse optional digits before decimal point ///
	double x = 0.0;
	while(notEOF() && isNumeric(current()))
	{
		//shift left (in base 10) and add new digit.
		x = x * 10.0 + (double)((int)current() - ASCII_ZERO_INT);
		currentpos++;
		reached_numerals = true;
	}

	/// Parse optional decimal point + digits ///
	if(parseChar(this->decimal_separator)) // parseChar('.'))
	{
		// Parse digits to right of decimal point
		double place_val = 0.1;
		while(notEOF() && isNumeric(current()))
		{
			x += place_val * (double)((int)current() - ASCII_ZERO_INT);
			currentpos++;
			place_val *= 0.1;
			reached_numerals = true;
		}
	}

	/// Parse optional exponent ///
	if(notEOF() && (current() == 'e' || current() == 'E' || current() == 'd' || current() == 'D'))
	{
		currentpos++;

		// Parse optional sign
		double exponent_sign = 1.;
		if(parseChar('+'))
		{}
		else if(parseChar('-'))
			exponent_sign = -1.;

		if(eof())
		{
			currentpos = initial_currentpos;
			return false;
		}


		// Parse exponent digits
		int e = 0;
		while(notEOF() && isNumeric(current()))
		{
			e = e * 10 + ((int)current() - ASCII_ZERO_INT);
			currentpos++;
		}

		result_out = sign * x * pow(10.0, exponent_sign * (double)e);
	}
	else
	{
		//no exponent
		result_out = (sign * x);
	}

	// Parse optional 'f' or 'F' (single-precision floating point number specifier)
	if(notEOF() && current() == 'f')
		parseChar('f');
	else if(notEOF() && current() == 'F')
		parseChar('F');

	return reached_numerals;*/
}


bool Parser::parseAlphaToken(std::string& token_out)
{
	token_out = "";
	bool found = false;
	for( ;notEOF() && ::isAlphabetic(text[currentpos]); ++currentpos)
	{
		token_out.push_back(text[currentpos]);
		found = true;
	}
	return found;
}


bool Parser::parseIdentifier(std::string& token_out)
{
	token_out = "";

	// Read first character of identifier
	if(notEOF() && (::isAlphabetic(text[currentpos]) || text[currentpos] == '_'))
	{
		token_out.push_back(text[currentpos]);
		currentpos++;
	}
	else
		return false;

	for( ;notEOF() && (::isAlphabetic(text[currentpos]) || isNumeric(text[currentpos]) || text[currentpos] == '_'); ++currentpos)
	{
		token_out.push_back(text[currentpos]);
	}
	return true;
}


bool Parser::parseNonWSToken(std::string& token_out)
{
	token_out = "";
	bool found = false;
	for( ;notEOF() && !::isWhitespace(text[currentpos]); ++currentpos)
	{
		token_out.push_back(text[currentpos]);
		found = true;
	}
	return found;
}


bool Parser::parseString(const std::string& s)
{
	const unsigned int initial_pos = currentPos();

	for(unsigned int i=0; i<s.length(); ++i)
	{
		if(eof() || current() != s[i])
		{
			currentpos = initial_pos;
			return false;
		}
		advance();
	}
	return true;
}


bool Parser::parseCString(const char* const s)
{
	const unsigned int initial_pos = currentPos();

	for(unsigned int i=0; s[i] != 0; ++i)
	{
		if(eof() || current() != s[i])
		{
			currentpos = initial_pos;
			return false;
		}
		advance();
	}
	return true;
}


//======================================================================== Unit Tests ========================================================================
#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include "../double-conversion/double-conversion.h"


static void testFailsToParseInt(const std::string& s)
{
	Parser p(s.c_str(), (unsigned int)s.length());

	int x;
	testAssert(!p.parseInt(x));
	testAssert(p.currentPos() == 0);
}


static void testParseInt(const std::string& s, int target)
{
	Parser p(s.c_str(), (unsigned int)s.length());

	int x;
	testAssert(p.parseInt(x));
	testAssert(p.currentPos() == s.length());
	testAssert(x == target);
}


static void testParseIntFromLongerString(const std::string& s, int target)
{
	Parser p(s.c_str(), (unsigned int)s.length());

	int x;
	testAssert(p.parseInt(x));
	testAssert(p.currentPos() <= s.length());
	testAssert(x == target);
}


static void testFailsToParseUnsignedInt(const std::string& s)
{
	Parser p(s.c_str(), (unsigned int)s.length());

	uint32 x;
	testAssert(!p.parseUnsignedInt(x));
	testAssert(p.currentPos() == 0);
}


static void testParseUnsignedInt(const std::string& s, uint32 target)
{
	Parser p(s.c_str(), (unsigned int)s.length());

	uint32 x;
	testAssert(p.parseUnsignedInt(x));
	testAssert(p.currentPos() == s.length());
	testAssert(x == target);
}


static void testParseUnsignedIntFromLongerString(const std::string& s, uint32 target)
{
	Parser p(s.c_str(), (unsigned int)s.length());

	uint32 x;
	testAssert(p.parseUnsignedInt(x));
	testAssert(p.currentPos() <= s.length());
	testAssert(x == target);
}


static void testFailsToParseFloat(const std::string& s)
{
	Parser p(s.c_str(), (unsigned int)s.length());

	float x;
	testAssert(!p.parseFloat(x));
	testAssert(p.currentPos() == 0);
}


static void testParseFloat(const std::string& s, float target)
{
	Parser p(s.c_str(), (unsigned int)s.length());

	float x;
	testAssert(p.parseFloat(x));
	testAssert(p.currentPos() == s.length());
	testAssert(x == target);
}


void Parser::doUnitTests()
{
	conPrint("Parser::doUnitTests()");

	//===================== parseChar ===============================
	{
		const std::string s = "a";
		Parser p(s.c_str(), (unsigned int)s.length());

		testAssert(p.parseChar('a'));
		testAssert(p.currentPos() == 1);
	}

	{
		const std::string s = "a";
		Parser p(s.c_str(), (unsigned int)s.length());

		testAssert(!p.parseChar('b'));
		testAssert(p.currentPos() == 0); // Should not advance if failed to parse char
	}

	

	// Perf test
	if(false)
	{
		{
			Timer timer;

			const std::string s = "123456789";

			const int N = 1000000;
			size_t sum = 0;
			for(int i=0; i<N; ++i)
			{
				Parser parser(s.c_str(), (unsigned int)s.length());
				uint32 x;
				parser.parseUnsignedInt(x);
				sum += x;
			}

			const double elapsed = timer.elapsed();
			printVar(sum);
			conPrint("parseUnsignedInt() per iter time: " + toString(1e9 * elapsed / N) + " ns");
		}

		{
			Timer timer;

			const std::string s = "   1";

			const int N = 1000000;
			size_t sum = 0;
			for(int i=0; i<N; ++i)
			{
				Parser parser(s.c_str(), (unsigned int)s.length());
				parser.parseWhiteSpace();
				sum += parser.currentPos();
			}

			const double elapsed = timer.elapsed();
			printVar(sum);
			conPrint("parseWhiteSpace() per iter time: " + toString(1e9 * elapsed / N) + " ns");
		}
	}


	//===================== parseInt ===============================

	// Test some failure cases
	testFailsToParseInt("");
	testFailsToParseInt(" ");
	testFailsToParseInt("a");
	testFailsToParseInt("aa");
	testFailsToParseInt("-");
	testFailsToParseInt("+");
	testFailsToParseInt("--");
	testFailsToParseInt("++");
	testFailsToParseInt(" 1");
	testFailsToParseInt(" -");
	testFailsToParseInt(" +");

	// Test some valid integers with non-numeric chars after integer
	for(int i=-10000; i<10000; ++i)
		testParseIntFromLongerString(::int32ToString(i) + "a", i);

	testParseInt("0", 0);
	testParseInt("-0", 0);
	testParseInt("-0", -0);

	for(int i=-10000; i<10000; ++i)
		testParseInt(::int32ToString(i), i);

	// NOTE: max representable signed 32 bit int is 2147483647
	testAssert(std::numeric_limits<int>::min() == -2147483647 - 1);
	testAssert(std::numeric_limits<int>::max() == 2147483647);

	// Test very large integer but valid 32-bit integers
	testParseInt("2147483639", 2147483639);
	testParseInt("2147483640", 2147483640);
	testParseInt("2147483641", 2147483641);
	testParseInt("2147483646", 2147483646);
	testParseInt("2147483647", 2147483647);

	// Test very large integers that are too large
	// NOTE: max representable signed 32 bit int is 2147483647
	testFailsToParseInt("2147483648");
	testFailsToParseInt("2147483649");
	testFailsToParseInt("2147483650");
	testFailsToParseInt("2147483651");
	testFailsToParseInt("2147483652");
	testFailsToParseInt("2147483653");
	testFailsToParseInt("2147483654");
	testFailsToParseInt("2147483655");
	testFailsToParseInt("2147483656");
	testFailsToParseInt("2147483657");
	testFailsToParseInt("2147483658");
	testFailsToParseInt("2147483659");


	testFailsToParseInt("21474836480");

	testFailsToParseInt("1000000000000");

	// Test negative ints

	// Test very large magnitude integers but valid 32-bit integers
	testParseInt("-2147483639", -2147483639);
	testParseInt("-2147483640", -2147483640);
	testParseInt("-2147483641", -2147483641);
	testParseInt("-2147483646", -2147483646);
	testParseInt("-2147483647", -2147483647);
	testParseInt("-2147483648", -2147483647 - 1); // NOTE: -2147483648 can't be written directly as 2147483648 is too large for a signed integer.

	// Test very large magnitude integers that are too large
	testFailsToParseInt("-2147483649");
	testFailsToParseInt("-2147483650");
	testFailsToParseInt("-2147483651");
	testFailsToParseInt("-2147483652");
	testFailsToParseInt("-2147483653");
	testFailsToParseInt("-2147483654");
	testFailsToParseInt("-2147483655");
	testFailsToParseInt("-2147483656");
	testFailsToParseInt("-2147483657");
	testFailsToParseInt("-2147483658");


	testFailsToParseInt("-21474836480");

	testFailsToParseInt("-1000000000000");



	//================== parseUnsignedInt =======================

	// Test some failure cases
	testFailsToParseUnsignedInt("");
	testFailsToParseUnsignedInt(" ");
	testFailsToParseUnsignedInt("a");
	testFailsToParseUnsignedInt("aa");
	testFailsToParseUnsignedInt("-");
	testFailsToParseUnsignedInt("+");
	testFailsToParseUnsignedInt("--");
	testFailsToParseUnsignedInt("++");
	testFailsToParseUnsignedInt(" 1");
	testFailsToParseUnsignedInt(" -");
	testFailsToParseUnsignedInt(" +");

	testFailsToParseUnsignedInt("-1");
	testFailsToParseUnsignedInt("-123");


	// Test some valid integers with non-numeric chars after integer
	for(int i=0; i<10000; ++i)
		testParseUnsignedIntFromLongerString(::int32ToString(i) + "a", i);

	// Test some valid uints
	testParseInt("0", 0);

	for(int i=0; i<10000; ++i)
		testParseUnsignedInt(::int32ToString(i), i);

	// Test very large integer but valid 32-bit integers
	// Note that 4294967295 is the largest representable 32 bit uint.
	testAssert(std::numeric_limits<unsigned int>::max() == 4294967295u);

	testParseUnsignedInt("4294967291", 4294967291u);
	testParseUnsignedInt("4294967292", 4294967292u);
	testParseUnsignedInt("4294967293", 4294967293u);
	testParseUnsignedInt("4294967294", 4294967294u);
	testParseUnsignedInt("4294967295", 4294967295u);

	// Test very large integers that are too large
	// NOTE: max representable unsigned 32 bit int is 4294967295
	testFailsToParseUnsignedInt("4294967296");
	testFailsToParseUnsignedInt("4294967297");
	testFailsToParseUnsignedInt("4294967298");
	testFailsToParseUnsignedInt("4294967299");
	testFailsToParseUnsignedInt("4294967300");
	testFailsToParseUnsignedInt("4294967301");
	testFailsToParseUnsignedInt("4294967302");
	testFailsToParseUnsignedInt("4294967303");
	testFailsToParseUnsignedInt("4294967304");
	testFailsToParseUnsignedInt("4294967305");

	testFailsToParseUnsignedInt("1000000000000");
	testFailsToParseUnsignedInt("1000000000000000");

	
	//================== parseFloat=======================

	//NOTE: general grammar for floats: [whitespace] [sign] [digits] [.digits] [ {d | D | e | E }[sign]digits]
	// We do not allow leading whitespace though.

	// Test some failure cases
	testFailsToParseFloat("");
	testFailsToParseFloat(" ");
	testFailsToParseFloat("a");
	testFailsToParseFloat("aa");
	testFailsToParseFloat("-");
	testFailsToParseFloat("+");
	testFailsToParseFloat("--");
	testFailsToParseFloat("++");
	testFailsToParseFloat(" 1");
	testFailsToParseFloat(" -");
	testFailsToParseFloat(" +");
	testFailsToParseFloat(" 1.0");

	// Positive integers
	testParseFloat("0", 0.0f);
	testParseFloat("1", 1.0f);
	testParseFloat("2", 2.0f);
	testParseFloat("3", 3.0f);

	// Positive integers with leading '+'
	testParseFloat("+0", 0.0f);
	testParseFloat("+1", 1.0f);
	testParseFloat("+2", 2.0f);
	testParseFloat("+3", 3.0f);

	// Negative integers
	testParseFloat("-0", -0.0f);
	testParseFloat("-1", -1.0f);
	testParseFloat("-2", -2.0f);
	testParseFloat("-3", -3.0f);

	// Positive floats
	testParseFloat("0.1", 0.1f);
	testParseFloat("1.1", 1.1f);
	testParseFloat("2.1", 2.1f);
	testParseFloat("3.1", 3.1f);

	// Positive floats with leading +
	testParseFloat("+0.1", 0.1f);
	testParseFloat("+1.1", 1.1f);
	testParseFloat("+2.1", 2.1f);
	testParseFloat("+3.1", 3.1f);

	// Negative floats
	testParseFloat("-0.1", -0.1f);
	testParseFloat("-1.1", -1.1f);
	testParseFloat("-2.1", -2.1f);
	testParseFloat("-3.1", -3.1f);

	// 'f' suffix
	testParseFloat("0.1f", 0.1f);
	testParseFloat("1.1f", 1.1f);
	testParseFloat("2.1f", 2.1f);
	testParseFloat("3.f", 3.f);

	// TODO: this should probably fail to parse:
	// testFailsToParseFloat("3f");

	// 'F' suffix
	testParseFloat("0.1F", 0.1F);
	testParseFloat("1.1F", 1.1F);
	testParseFloat("2.1F", 2.1F);
	testParseFloat("3.1F", 3.1F);

	// Check exponent notation
	testParseFloat("1.23e30", 1.23e30f);
	testParseFloat("1.23E30", 1.23e30f);
	testParseFloat("1.23e+30f", 1.23e30f);
	testParseFloat("1.23E+30F", 1.23e30f);
	testParseFloat("1.23e-30", 1.23e-30f);
	testParseFloat("1.23E-30", 1.23e-30f);
	testParseFloat("1.23e-30f", 1.23e-30f);
	testParseFloat("1.23E-30F", 1.23e-30f);
	testParseFloat("-1.23e-30", -1.23e-30f);
	testParseFloat("-1.23E-30", -1.23e-30f);
	testParseFloat("-1.23e-30f", -1.23e-30f);
	testParseFloat("-1.23E-30F", -1.23e-30f);


	//================== parseString =======================
	{
		const std::string s = "Hello World";
		{
		Parser p(s.c_str(), (unsigned int)s.size());
		testAssert(p.parseString("Hello World"));
		testAssert(p.eof());
		}
		{
		Parser p(s.c_str(), (unsigned int)s.size());
		testAssert(p.parseString("Hello"));
		testAssert(p.currentPos() == 5);
		}
		{
		Parser p(s.c_str(), (unsigned int)s.size());
		testAssert(!p.parseString("AHello"));
		testAssert(p.currentPos() == 0);
		}
		{
		Parser p(s.c_str(), (unsigned int)s.size());
		testAssert(!p.parseString("Hello World AA"));
		testAssert(p.currentPos() == 0);
		}
	}

	//================== parseCString =======================
	{
		const std::string s = "Hello World";
		{
		Parser p(s.c_str(), (unsigned int)s.size());
		testAssert(p.parseCString("Hello World"));
		testAssert(p.eof());
		}
		{
		Parser p(s.c_str(), (unsigned int)s.size());
		testAssert(p.parseCString("Hello"));
		testAssert(p.currentPos() == 5);
		}
		{
		Parser p(s.c_str(), (unsigned int)s.size());
		testAssert(!p.parseCString("AHello"));
		testAssert(p.currentPos() == 0);
		}
		{
		Parser p(s.c_str(), (unsigned int)s.size());
		testAssert(!p.parseCString("Hello World AA"));
		testAssert(p.currentPos() == 0);
		}
	}

	//================== parseWhiteSpace =======================

	{
		const std::string s = "";
		Parser p(s.c_str(), (unsigned int)s.size());
		testAssert(!p.parseWhiteSpace());
		testAssert(p.currentPos() == 0);
	}

	{
		const std::string s = " ";
		Parser p(s.c_str(), (unsigned int)s.size());
		testAssert(p.parseWhiteSpace());
		testAssert(p.currentPos() == 1);
	}

	{
		const std::string s = " \t\r\n";
		Parser p(s.c_str(), (unsigned int)s.size());
		testAssert(p.parseWhiteSpace());
		testAssert(p.currentPos() == 4);
	}

	{
		const std::string s = "a";
		Parser p(s.c_str(), (unsigned int)s.size());
		testAssert(!p.parseWhiteSpace());
		testAssert(p.currentPos() == 0);
	}

	{
		const std::string s = " a ";
		Parser p(s.c_str(), (unsigned int)s.size());
		testAssert(p.parseWhiteSpace());
		testAssert(p.currentPos() == 1);
		testAssert(p.parseChar('a'));
		testAssert(p.parseWhiteSpace());
		testAssert(p.currentPos() == 3);
	}

	/*char buffer[512];
	conPrint("Doing functionality tests..");
	int N = 1000000;
	for(int i=0; i<N; ++i)
	{
		double x = i * 0.001;


		//================ Test built-in C functions ==================
		{
			// Convert double to string with sprintf
			sprintf(buffer, "%1.32f", x);
			std::string s1(buffer);

			// Convert string back to double
			double x2 = stringToDouble(std::string(buffer));

			// Check the original value is the same as the new value
			testAssert(x == x2);
		}

		//=============== Test Google's double-to-string code ======================
		{
			// Convert double to string with Google's code
			double_conversion::DoubleToStringConverter converter(
				double_conversion::DoubleToStringConverter::NO_FLAGS,
				"Inf",
				"NaN",
				'E',
				-6, // decimal_in_shortest_low
				21, // decimal_in_shortest_high
				6, // max_leading_padding_zeroes_in_precision_mode
				1 // max_trailing_padding_zeroes_in_precision_mode
			);

			double_conversion::StringBuilder builder(buffer, sizeof(buffer));

			converter.ToShortest(x, &builder);

			std::string s2(builder.Finalize());

			// Convert back from string to double
			double x2 = stringToDouble(s2);

			// Check the original value is the same as the new value
			testAssert(x == x2);

			// Convert back from string to double with Google's code
			double_conversion::StringToDoubleConverter s2d_converter(
				double_conversion::StringToDoubleConverter::ALLOW_LEADING_SPACES,
				0.0,
				0.0,
				"infinity",
				"NaN"
			);

			int num_processed_chars = 0;
			double x3 = s2d_converter.StringToDouble(s2.c_str(), (int)s2.length(), &num_processed_chars);
			testAssert(x3 == x);
		}
	}

	conPrint("\tDone.");




	// Speedtest
	

	conPrint("stringToDouble");
	{
		Timer t;

		double sum = 0;
		for(int i=0; i<N; ++i)
		{
			double x = i * 0.001;

			// Convert double to string
			//std::string s = ::toString(x);

			char buffer[512]; // Double can be up to ~1x10^38 :)
			sprintf(buffer, "%1.32f", x);


			// Convert string back to double
			double x2 = stringToDouble(std::string(buffer));
			//testAssert(x == x2);

			sum += x2;
		}
		conPrint(t.elapsedString());
		conPrint(::toString(sum));
	}
	conPrint("");

	conPrint("double_conversion");
	{
		Timer t;

		double sum = 0;
		for(int i=0; i<N; ++i)
		{
			double x = i * 0.001;

			// Convert double to string
			char buffer[512]; // Double can be up to ~1x10^38 :)
			sprintf(buffer, "%1.32f", x);
			std::string s(buffer);

			double_conversion::StringToDoubleConverter converter(
				double_conversion::StringToDoubleConverter::ALLOW_LEADING_SPACES,
				0.0,
				0.0,
				"infinity",
				"NaN"
			);

			//double x = stringToFloat(text);
			int num_processed_chars = 0;
			double x2 = converter.StringToDouble(s.c_str(), (int)s.length(), &num_processed_chars);
			//testAssert(x == x2);

			sum += x2;
		}
		conPrint(t.elapsedString());
		conPrint(::toString(sum));
	}
	conPrint("");

	conPrint("Parser");
	{
		//const std::string text = "123.456E+2";

		Timer t;

		double sum = 0;
		for(int i=0; i<N; ++i)
		{
			double x = i * 0.001;

			// Convert double to string
			char buffer[512]; // Double can be up to ~1x10^38 :)
			sprintf(buffer, "%1.32f", x);
			std::string s(buffer);

			// Convert string back to double
			Parser p(s.c_str(), (unsigned int)s.size());
			double x2;
			p.parseDouble(x2);

			//testAssert(x == x2);
			sum += x2;
		}
		conPrint(t.elapsedString());
		conPrint(::toString(sum));
	}
	conPrint("");
	*/
	// Speedtest
	/*{
		const std::string text = "123.456";

		Timer t;

		Parser p;

		double y = 0;
		for(int i=0; i<1000000; ++i)
		{
			p.reset(text.c_str(), (unsigned int)text.size());

			double x;
			p.parseDouble(x);
			y += x;
		}
		conPrint(t.elapsedString());
		conPrint(::toString(y));
	}*/

	
	// Test parseFloat()
	{
		std::string text = "0";

		// Note that if we compile with floating point model fast (/fp:fast),
		// then -0 and 0 are treated the same, and "0" will get parsed as -0.

		Parser p(text.c_str(), (unsigned int)text.size());
		
		double x = 0;
		testAssert(p.parseDouble(x));
		testAssert(p.currentPos() == 1);

		testAssert(x == -0.0 || x == 0.0);
	}

#ifdef _DEBUG
	{
	std::string text = "-456.5456e21 673.234 -0.5e-13 .6 167/2/3 4/5/6";

	// Test parseFloat()
	{
		Parser p(text.c_str(), (unsigned int)text.size());
		float f;
		assert(p.parseFloat(f));
		assert(f == -456.5456e21f);
		assert(p.parseWhiteSpace());

		assert(p.parseFloat(f));
		assert(f == 673.234f);
		assert(p.parseWhiteSpace());

		assert(p.parseFloat(f));
		assert(f == -0.5e-13f);
		assert(p.parseWhiteSpace());

		assert(p.parseFloat(f));
		assert(f == .6f);
		assert(p.parseWhiteSpace());
	}
	
	// Test parseDouble()
	{
		Parser p(text.c_str(), (unsigned int)text.size());
		double f;
		assert(p.parseDouble(f));
		assert(f == -456.5456e21);
		assert(p.parseWhiteSpace());

		assert(p.parseDouble(f));
		assert(f == 673.234);
		assert(p.parseWhiteSpace());

		assert(p.parseDouble(f));
		assert(f == -0.5e-13);
		assert(p.parseWhiteSpace());

		assert(p.parseDouble(f));
		assert(f == .6);
		assert(p.parseWhiteSpace());
	}

	}
	{
		std::string text = "167/2/3 4/5/6";

		Parser p(text.c_str(), (unsigned int)text.size());
		unsigned int x;
		assert(p.parseUnsignedInt(x));
		assert(x == 167);
		assert(p.parseChar('/'));
		assert(p.parseUnsignedInt(x));
		assert(x == 2);
		assert(p.parseChar('/'));

		assert(p.parseUnsignedInt(x));
		assert(x == 3);

		assert(p.parseWhiteSpace());

		assert(p.parseUnsignedInt(x));
		assert(x == 4);
		assert(p.parseChar('/'));
		assert(p.parseUnsignedInt(x));
		assert(x == 5);
		assert(p.parseChar('/'));
		assert(p.parseUnsignedInt(x));
		assert(x == 6);
		assert(p.eof());

		assert(!p.parseChar('/'));
		assert(!p.parseUnsignedInt(x));
		assert(!p.parseWhiteSpace());
	}

	// Check parsing of 'f' suffix
	{
		const std::string text = "123.456e3f";
		Parser p(text.c_str(), (unsigned int)text.size());

		double x;
		assert(p.parseDouble(x));
		assert(::epsEqual((float)x, 123.456e3f));
		assert(p.eof());
	}

	// Try German locale where decimal separtor is ','
	/*{
		const char* result = std::setlocale(LC_ALL, "german");
		assert(result);

		const std::string text = "123,456";
		Parser p(text.c_str(), text.size());
		double x;
		assert(p.parseDouble(x));
		
		assert(::epsEqual(x, 123.456));
	}

	// Reset locale to default locale
	{
		const char* result = std::setlocale(LC_ALL, "");
		assert(result);
	}*/


	/*{
	const std::string text = "123--456222";

	Parser p(text.c_str(), (unsigned int)text.size());

	unsigned int x;
	assert(p.parseNDigitUnsignedInt(3, x));
	assert(x == 123);
	
	assert(p.parseChar('-'));
	assert(p.parseChar('-'));

	assert(p.parseNDigitUnsignedInt(3, x));
	assert(x == 456);

	assert(p.parseNDigitUnsignedInt(3, x));
	assert(x == 222);
	}*/

	{
	const std::string text = "bleh 123";
	Parser p(text.c_str(), (unsigned int)text.size());

	double x;
	assert(!p.parseDouble(x));
	}

	{
	const std::string text = "123 -645 meh";
	Parser p(text.c_str(), (unsigned int)text.size());

	int x;
	assert(p.parseInt(x));
	assert(x == 123);

	assert(p.parseWhiteSpace());

	assert(p.parseInt(x));
	assert(x == -645);
	
	assert(p.parseWhiteSpace());

	assert(!p.parseInt(x));
	}

	{
	const std::string text = "BLEH";
	Parser p(text.c_str(), (unsigned int)text.size());

	assert(!p.parseString("BLEHA"));
	assert(p.parseString("BL"));
	assert(p.parseString("EH"));
	}

	{
		const std::string text = ".";
		Parser p(text.c_str(), (unsigned int)text.size());

		assert(!p.fractionalNumberNext());
	}
	{
		const std::string text = "1.3";
		Parser p(text.c_str(), (unsigned int)text.size());

		assert(p.fractionalNumberNext());
	}
	{
		const std::string text = "13";
		Parser p(text.c_str(), (unsigned int)text.size());

		assert(!p.fractionalNumberNext());
	}
	{
		const std::string text = "4e-5f";
		Parser p(text.c_str(), (unsigned int)text.size());

		assert(p.fractionalNumberNext());

		double x;
		bool res = p.parseDouble(x);
		assert(res);
		assert(::epsEqual((float)x, 4e-5f));
	}
#endif
	conPrint("Done.");
}


#endif // BUILD_TESTS
