/*=====================================================================
Parser.cpp
----------
File created by ClassTemplate on Sat Jul 08 20:49:28 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "Parser.h"



#include "../maths/mathstypes.h"


Parser::Parser(char* text_, unsigned int textsize_)//const std::string& text_)
:	text(text_),
	textsize(textsize_)
{
	currentpos = 0;
	//textsize = text.size();
}


Parser::~Parser()
{
}










static const int ASCII_ZERO_INT = (int)'0';



bool Parser::parseFloat(float& result_out)
{
	double x;
	const bool result = parseDouble(x);
	result_out = x;
	return result;



	/*for(unsigned int i=0 ;notEOF() && !::isWhitespace(text[currentpos]) && i < PARSER_TMP_BUF_SIZE; ++currentpos, ++i)
		tmp[i] = text[currentpos];
	tmp[i] = '\0';

	result_out = (float)atof(tmp);
	return true;*/










	/*const char* start = text + currentpos;

	for(;notEOF(); ++currentpos)
	{
		if(isWhitespace(text[currentpos]))
		{
			char old = text[currentpos];
			text[currentpos] = '\0';//evil hax >:)	
			result_out = (float)atof(start);
			text[currentpos] = old;
			return true;
		}
	}
	return false;*/











	/*const char* startptr = text + currentpos;
	char* endptr;
	result_out = (float)strtod(startptr, &endptr);

	currentpos = endptr - text;

	return startptr != endptr;*/

	//char tmp[256];
/*
	for(unsigned int i=0 ;notEOF() && !::isWhitespace(text[currentpos]); ++currentpos, ++i)
		tmp[i] = text[currentpos];
	tmp[i] = '\0';
	

	result_out = (float)atof(tmp);
	//find next space
	//for( ;notEOF() && !::isWhitespace(text[currentpos]); ++currentpos)
	//{}
	//currentpos += i;

	return true;
*/


	
	
	/*const bool result = parseNonWSToken(temp);
	if(!result)
		return false;

	result_out = (float)atof(temp.c_str());
	return true;*/
	
	
	
	/*double d = 0.0;
	if(parseChar('+'))
	{}
	else if(parseChar('-'))
	{
		d *= -1.0;
	}

	for( ;currentpos < textsize && ::isNumeric(text[currentpos]); ++currentpos)
	{
		x = 10*x + ((int)text[currentpos] - (int)'0');
		found_int = true;
	}*/



	/*if(eof())
		return false;

	char* p = (char*)&text[currentpos];
	const int numread = sscanf(p, "%f", &result_out);

	/// parse sign
	if(parseChar('+') || parseChar('-'))
	{}

	int x;
	parseInt(x);
	if(parseChar('.'))
	{
		parseInt(x);
		if(parseChar('d') || parseChar('D') || parseChar('e') || parseChar('E'))
		{
			/// parse sign
			if(parseChar('+') || parseChar('-'))
			{}

			parseInt(x);
		}
	}

	
	return numread == 1;*/



	/*std::string s;
	if(parseChar('+'))
	{}
	else if(parseChar('-'))
	{
		concatWithChar(s, '-');
	}*/
	

}

//[whitespace] [sign] [digits] [.digits] [ {d | D | e | E }[sign]digits]

//must be whitespace delimited
bool Parser::parseDouble(double& result_out)
{
	if(eof())
		return false;
	
	/// Parse sign ///
	double sign = 1.;
	
	if(parseChar('+'))
	{}
	else if(parseChar('-'))
		sign = -1.;

	if(eof())// || !isNumeric(current()))
		return false;

	/// Parse optional digits before decimal point ///
	double x = 0.0;
	while(notEOF() && isNumeric(current()))
	{
		//shift left (in base 10) and add new digit.
		x = x * 10.0 + (double)((int)current() - ASCII_ZERO_INT);
		currentpos++;
	}

	/// Parse optional decimal point + digits ///
	if(parseChar('.'))
	{
		//parse digits to right of decimal point
		double place_val = 0.1;
		while(notEOF() && isNumeric(current()))
		{
			x += place_val * (double)((int)current() - ASCII_ZERO_INT);
			currentpos++;
			place_val *= 0.1;
		}
	}

	/// Parse optional exponent ///
	if(notEOF() && (current() == 'e' || current() == 'E' || current() == 'd' || current() == 'D'))
	{
		currentpos++;

		//parse optional sign
		double exponent_sign = 1.;
		if(parseChar('+'))
		{}
		else if(parseChar('-'))
			exponent_sign = -1.;

		if(eof())
			return false;

		//parse exponent digits
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

	return true;

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



/*bool Parser::parseString(const std::string& s)
{
	if(currentpos + s.size() >= textsize)
		return false;

	for(unsigned int i=0; i<s.size(); ++i)
}*/

void Parser::doUnitTests()
{
#ifdef DEBUG
	std::string text = "-456.5456e21 673.234 -0.5e-53 .6 167/2/3 4/5/6";

	Parser p((char*)text.c_str(), text.size());

	float f;
	assert(p.parseFloat(f));
	assert(::epsEqual((float)f, -456.5456e21f));
	assert(p.parseWhiteSpace());

	assert(p.parseFloat(f));
	assert(::epsEqual((float)f, 673.234f));
	assert(p.parseWhiteSpace());

	assert(p.parseFloat(f));
	assert(::epsEqual((float)f, -0.5e-53f));
	assert(p.parseWhiteSpace());

	assert(p.parseFloat(f));
	assert(::epsEqual((float)f, .6f));
	assert(p.parseWhiteSpace());

	unsigned int x;
	assert(p.parseInt(x));
	assert(x == 167);
	assert(p.parseChar('/'));
	assert(p.parseInt(x));
	assert(x == 2);
	assert(p.parseChar('/'));

	assert(p.parseInt(x));
	assert(x == 3);

	assert(p.parseWhiteSpace());

	assert(p.parseInt(x));
	assert(x == 4);
	assert(p.parseChar('/'));
	assert(p.parseInt(x));
	assert(x == 5);
	assert(p.parseChar('/'));
	assert(p.parseInt(x));
	assert(x == 6);
	assert(p.eof());

	assert(!p.parseChar('/'));
	assert(!p.parseInt(x));
	assert(!p.parseWhiteSpace());


#endif
}









