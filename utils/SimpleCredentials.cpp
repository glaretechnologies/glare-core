/*=====================================================================
SimpleCredentials.cpp
---------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "SimpleCredentials.h"


#include "FileUtils.h"
#include "Parser.h"


// Throws glare::Exception on failure.
SimpleCredentials SimpleCredentials::parseCredentials(const std::string& credentials_path)
{
	const std::string contents = FileUtils::readEntireFileTextMode(credentials_path);

	SimpleCredentials creds;

	Parser parser(contents);

	while(!parser.eof())
	{
		string_view key, value;
		parser.parseToCharOrEOF(':', key);
		if(parser.eof())
			break;

		if(!parser.parseChar(':'))
			throw glare::Exception("Error parsing ':' from '" + credentials_path + "'.");

		parser.parseWhiteSpace();
		parser.parseLine(value);

		creds.creds[::stripHeadAndTailWhitespace(toString(key))] = ::stripHeadAndTailWhitespace(toString(value));
	}

	return creds;
}
