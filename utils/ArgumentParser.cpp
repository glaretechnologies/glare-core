/*=====================================================================
ArgumentParser.cpp
------------------
File created by ClassTemplate on Sat Nov 03 18:21:05 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "ArgumentParser.h"

#include <assert.h>
#include "../utils/stringutils.h"

ArgumentParser::ArgumentParser(const std::vector<std::string>& args_, const std::map<std::string, int>& syntax_)
:	args(args_),
	syntax(syntax_)
{
	
	// Parse

	bool parsed_free_arg = false;
	for(int i=1; i<(int)args.size(); ++i)
	{
		const std::string name = args[i];
		if(syntax.find(name) == syntax.end())
		{
			if(parsed_free_arg)
				throw ArgumentParserExcep("Unknown option '" + name + "'");

			parsed_free_arg = true;
			unnamed_arg = args[i];
		}

		const int numargs = syntax[name];
		/*assert(numargs == 0 || numargs == 1);
		if(numargs == 1)
		{
			i++;
			if(i >= (int)args.size())
				throw ArgumentParserExcep("Failed to find expected token following '" + name + "'");

			parsed_args[name] = args[i];
		}
		else
		{
			parsed_args[name] = "";
		}*/
		parsed_args[name].resize(numargs);
		for(int z=0; z<numargs; ++z)
		{
			i++;
			if(i >= (int)args.size())
				throw ArgumentParserExcep("Failed to find expected token following '" + name + "'");

			parsed_args[name][z] = args[i];
		}
	}
}


ArgumentParser::~ArgumentParser()
{
	
}

const std::string ArgumentParser::getArgValue(const std::string& name, unsigned int value_index) const
{
	if(!isArgPresent(name))
		throw ArgumentParserExcep("arg with name '" + name + "' not present.");

	const std::vector<std::string>& values = (*parsed_args.find(name)).second;
	if(value_index >= values.size())
		throw ArgumentParserExcep("value_index out of bounds");
	return values[value_index];
}
	
int ArgumentParser::getArgIntValue(const std::string& name, unsigned int value_index) const
{
	return stringToInt(getArgValue(name, value_index));
}
	
double ArgumentParser::getArgDoubleValue(const std::string& name, unsigned int value_index) const
{
	return stringToDouble(getArgValue(name, value_index));
}




