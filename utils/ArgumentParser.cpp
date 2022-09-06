/*=====================================================================
ArgumentParser.cpp
------------------
Copyright Glare Technologies Limited 2021 - 
=====================================================================*/
#include "ArgumentParser.h"


#include "StringUtils.h"
#include <assert.h>


ArgumentParser::ArgumentParser(const std::vector<std::string>& args, const std::map<std::string, std::vector<ArgumentType> >& syntax_, bool allow_unnamed_arg)
:	syntax(syntax_)
{
	// Parse
	bool parsed_free_arg = false;
	for(int i=1; i<(int)args.size(); ++i)
	{
		const std::string name = args[i];
		if(syntax.find(name) == syntax.end())
		{
			if(!allow_unnamed_arg || parsed_free_arg)
				throw ArgumentParserExcep("Unknown option '" + name + "'");

			parsed_free_arg = true;
			unnamed_arg = args[i];
		}
		else
		{
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
		}
	}
}


ArgumentParser::ArgumentParser()
{}


ArgumentParser::~ArgumentParser()
{}


const std::vector<std::string> ArgumentParser::getArgs() const
{
	std::vector<std::string> res;

	if(unnamed_arg != "")
		res.push_back(unnamed_arg);

	for(std::map<std::string, std::vector<ParsedArg> >::const_iterator i = parsed_args.begin(); i != parsed_args.end(); ++i)
	{
		res.push_back((*i).first);
		for(unsigned int z=0; z<(*i).second.size(); ++z)
			res.push_back((*i).second[z].toString());
	}
	return res;
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


const std::string ArgumentParser::getArgsAsString() const
{
	return StringUtils::join(getArgs(), " ");
}


void ArgumentParser::appendToOrCreateArg(const std::string& name, const std::string& value)
{
	std::map<std::string, std::vector<ParsedArg> >::iterator res = parsed_args.find(name);
	if(res == parsed_args.end())
	{
		ParsedArg p;
		p.type = ArgumentType_string;
		p.string_val = value;
		parsed_args[name] = std::vector<ParsedArg>(1, p);
	}
	else
	{
		(*res).second[0].string_val += value;
	}
}


void ArgumentParser::setStringArg(const std::string& name, const std::string& s)
{
	ParsedArg p;
	p.type = ArgumentType_string;
	p.string_val = s;
	parsed_args[name] = std::vector<ParsedArg>(1, p);
}


void ArgumentParser::removeArg(const std::string& name)
{
	parsed_args.erase(name);
}


const std::string ArgumentParser::ParsedArg::toString() const
{
	switch(type)
	{
	case ArgumentType_string:
		return string_val;
	case ArgumentType_int:
		return ::toString(int_val);
	case ArgumentType_double:
		return ::toString(double_val);
	default:
		assert(0);
		return "ERROR";
	};
}
