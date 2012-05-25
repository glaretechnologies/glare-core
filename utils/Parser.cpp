/*=====================================================================
Parser.cpp
----------
Copyright Glare Technologies Limited 2009 - 
=====================================================================*/
#include "Parser.h"


#include <cmath>
#include <clocale>
#include <cstring>
#include "../indigo/globals.h"
#include "../utils/stringutils.h"
#include "../utils/timer.h"
#include "../maths/mathstypes.h"
#include "../double-conversion/double-conversion.h"


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


static const int ASCII_ZERO_INT = (int)'0';


bool Parser::parseInt(int& result_out)
{
	const unsigned int initial_currentpos = currentpos;

	/// Parse sign ///
	int sign = 1;
	if(parseChar('+'))
	{}
	else if(parseChar('-'))
		sign = -1;

	bool valid = false;
	unsigned int x = 0;
	//const int initial_currentpos = currentpos;
	for( ;notEOF() && ::isNumeric(text[currentpos]); ++currentpos)
	{
		x = 10*x + ((int)text[currentpos] - ASCII_ZERO_INT);
		valid = true;
	}

	result_out = sign * x;

	if(!valid)
		currentpos = initial_currentpos; // Restore currentpos.
	
	return valid; //currentpos - initial_currentpos > 0;
}


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

	/*double x;
	const bool result = parseDouble(x);
	result_out = (float)x;
	return result;*/
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
		::concatWithChar(token_out, text[currentpos]);
		found = true;
	}
	return found;
}

bool Parser::parseIdentifier(std::string& token_out)
{
	token_out = "";
	//bool found = false;

	// Read first character of identifier
	if(notEOF() && (::isAlphabetic(text[currentpos]) || text[currentpos] == '_'))
	{
		::concatWithChar(token_out, text[currentpos]);
		currentpos++;
		//found = true;
	}
	else
		return false;

	for( ;notEOF() && (::isAlphabetic(text[currentpos]) || isNumeric(text[currentpos]) || text[currentpos] == '_'); ++currentpos)
	{
		::concatWithChar(token_out, text[currentpos]);
		//found = true;
	}
	return true;//found;
}

bool Parser::parseNonWSToken(std::string& token_out)
{
	token_out = "";
	bool found = false;
	for( ;notEOF() && !::isWhitespace(text[currentpos]); ++currentpos)
	{
		::concatWithChar(token_out, text[currentpos]);
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


/*bool Parser::parseString(const std::string& s)
{
	if(currentpos + s.size() >= textsize)
		return false;

	for(unsigned int i=0; i<s.size(); ++i)
}*/


/*template <class Real>
inline bool epsEqual(Real a, Real b, Real epsilon = 0.00001f)
{
	return fabs(a - b) <= epsilon;
}*/


#if BUILD_TESTS


#include "../double-conversion/double-conversion.h"
#include "../indigo/TestUtils.h"


void Parser::doUnitTests()
{

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


	{
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
	}

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
}


#endif
