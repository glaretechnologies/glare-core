/*=====================================================================
ArgumentParser.cpp
------------------
File created by ClassTemplate on Sat Nov 03 18:21:05 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "ArgumentParser.h"

#include <assert.h>
#include "../utils/stringutils.h"

ArgumentParser::ArgumentParser(const std::vector<std::string>& args_, const std::map<std::string, std::vector<ArgumentType> >& syntax_)
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

		const std::vector<ArgumentType>& arg_types = syntax[name];

		parsed_args[name] = std::vector<ParsedArg>();

		for(unsigned int z=0; z<arg_types.size(); ++z)
		{
			i++;
			if(i >= (int)args.size())
				throw ArgumentParserExcep("Failed to find expected token following '" + name + "'");

			try
			{
				parsed_args[name].push_back(ParsedArg());
				if(arg_types[z] == ArgumentType_string)
				{
					parsed_args[name].back().string_val = args[i];
				}
				else if(arg_types[z] == ArgumentType_int)
				{
					parsed_args[name].back().int_val = ::stringToInt(args[i]);
				}
				else if(arg_types[z] == ArgumentType_double)
				{
					parsed_args[name].back().double_val = ::stringToDouble(args[i]);
				}
				else
				{
					assert(0);
				}

				parsed_args[name].back().type = arg_types[z];
			}
			catch(StringUtilsExcep& e)
			{
				throw ArgumentParserExcep(e.what());
			}
		}

				



		/*const int numargs = syntax[name];
		parsed_args[name].resize(numargs);
		for(int z=0; z<numargs; ++z)
		{
			i++;
			if(i >= (int)args.size())
				throw ArgumentParserExcep("Failed to find expected token following '" + name + "'");

			parsed_args[name][z] = args[i];
		}*/
	}
}


ArgumentParser::~ArgumentParser()
{
	
}

const std::string ArgumentParser::getArgStringValue(const std::string& name, unsigned int value_index) const
{
	if(!isArgPresent(name))
		throw ArgumentParserExcep("arg with name '" + name + "' not present.");

	const std::vector<ParsedArg>& values = (*parsed_args.find(name)).second;

	if(value_index >= values.size())
		throw ArgumentParserExcep("value_index out of bounds");

	if(values[value_index].type != ArgumentType_string)
		throw ArgumentParserExcep("incorrect type");

	return values[value_index].string_val;
}
	
int ArgumentParser::getArgIntValue(const std::string& name, unsigned int value_index) const
{
	if(!isArgPresent(name))
		throw ArgumentParserExcep("arg with name '" + name + "' not present.");

	const std::vector<ParsedArg>& values = (*parsed_args.find(name)).second;

	if(value_index >= values.size())
		throw ArgumentParserExcep("value_index out of bounds");

	if(values[value_index].type != ArgumentType_int)
		throw ArgumentParserExcep("incorrect type");

	return values[value_index].int_val;
}
	
double ArgumentParser::getArgDoubleValue(const std::string& name, unsigned int value_index) const
{
	if(!isArgPresent(name))
		throw ArgumentParserExcep("arg with name '" + name + "' not present.");

	const std::vector<ParsedArg>& values = (*parsed_args.find(name)).second;

	if(value_index >= values.size())
		throw ArgumentParserExcep("value_index out of bounds");

	if(values[value_index].type != ArgumentType_double)
		throw ArgumentParserExcep("incorrect type");

	return values[value_index].double_val;
}


const std::string ArgumentParser::getOriginalArgsAsString() const
{
	return StringUtils::join(getOriginalArgs(), " ");
}

