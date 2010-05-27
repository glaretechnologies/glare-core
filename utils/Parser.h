/*=====================================================================
Parser.h
--------
Copyright Glare Technologies Limited 2009 - 
=====================================================================*/
#ifndef __PARSER_H_666_
#define __PARSER_H_666_


#include "../utils/stringutils.h"
#include <string>


/*=====================================================================
Parser
------

=====================================================================*/
class Parser
{
public:
	/*=====================================================================
	Parser
	------
	
	=====================================================================*/
	Parser();
	Parser(const char* text, unsigned int textsize);

	~Parser();

	void reset(const char* text, unsigned int textsize);

	//returns if character was found
	//if it was, currentpos will be pointing to the next character.
	//if not, it won't be moved.
	inline bool parseChar(char target);
	bool parseInt(int& result_out);
	inline bool parseUnsignedInt(unsigned int& result_out);
	inline bool parseNDigitUnsignedInt(unsigned int N, unsigned int& result_out);
	bool parseFloat(float& result_out);
	bool parseDouble(double& result_out);
	inline bool parseWhiteSpace();
	inline void parseSpacesAndTabs();
	inline void advancePastLine();
	inline void parseLine();
	//bool parseString(const std::string& s);
	bool parseAlphaToken(std::string& token_out);
	bool parseIdentifier(std::string& token_out);
	bool parseNonWSToken(std::string& token_out);
	bool parseString(const std::string& s);

	bool fractionalNumberNext();

	static void doUnitTests();

	//Only defined if eof() is not true.
	inline char current() const;
	inline unsigned int currentPos() const { return currentpos; }

	inline bool eof() const;
	inline bool notEOF() const;
	inline void advance();

	inline bool nextIsEOF() const { return currentpos + 1 >= textsize; }
	inline bool nextIsNotEOF() const { return currentpos + 1 < textsize; }
	inline char next() const { return text[currentpos + 1]; }
	inline bool nextIsChar(char c) const { return nextIsNotEOF() && next() == c; }

	inline const char* getText() const { return text; }
	inline unsigned int getTextSize() const { return textsize; }
private:
	const char* text;
	unsigned int currentpos;
	unsigned int textsize;
	char decimal_separator;
};




bool Parser::parseChar(char target)
{
	if(currentpos < textsize && text[currentpos] == target)
	{
		++currentpos;
		return true;
	}
	else
		return false;
}


bool Parser::parseUnsignedInt(unsigned int& result_out)
{
	unsigned int x = 0;
	const int initial_currentpos = currentpos;
	for( ;notEOF() && ::isNumeric(text[currentpos]); ++currentpos)
		x = 10*x + ((int)text[currentpos] - (int)'0');
	result_out = x;
	return currentpos - initial_currentpos > 0;
}

bool Parser::parseNDigitUnsignedInt(unsigned int N, unsigned int& result_out)
{
	unsigned int x = 0;
	const int initial_currentpos = currentpos;
	for( ;notEOF() && ::isNumeric(text[currentpos]) && (currentpos - initial_currentpos < N); ++currentpos)
		x = 10*x + ((int)text[currentpos] - (int)'0');
	result_out = x;
	return currentpos - initial_currentpos == N;
}

bool Parser::parseWhiteSpace()
{
	bool found = false;
	for( ;notEOF() && ::isWhitespace(text[currentpos]); ++currentpos)
		found = true;

	return found;
}


void Parser::parseSpacesAndTabs()
{
	for( ;notEOF() && (text[currentpos] == ' ' || text[currentpos] == '\t') ; ++currentpos)
	{}
}
/*
bool Parser::parseFloat(float& result_out)
{
	for(unsigned int i=0 ;notEOF() && !::isWhitespace(text[currentpos]) && i < PARSER_TMP_BUF_SIZE; ++currentpos, ++i)
		tmp[i] = text[currentpos];
	tmp[i] = '\0';
	
	result_out = (float)atof(tmp);
	return true;
}*/



void Parser::advancePastLine()
{
	for( ; !eof(); ++currentpos)
	{
		/*if(current() == '\n' || current() == '\r')
		{
			currentpos++;
			return;
		}*/
		if(current() == '\n')//line feed (Unix)
		{
			currentpos++;
			return;
		}
		else if(current() == '\r')//carriage return
		{
			currentpos++;
			if(current() == '\n')
			{
				//assume we are seeing a CRLF on windows.  Treat as one new line.
				currentpos++;
			}
			return;
		}
	}
}


void Parser::parseLine()
{
	for( ; notEOF() && current() != '\n' && current() != '\r'; ++currentpos)
	{
		currentpos++;
	}
}


char Parser::current() const
{
	return text[currentpos];
}


bool Parser::eof() const
{
	return currentpos >= textsize;
}


bool Parser::notEOF() const
{
	return currentpos < textsize;
}


void Parser::advance()
{
	currentpos++;
}


#endif //__PARSER_H_666_
