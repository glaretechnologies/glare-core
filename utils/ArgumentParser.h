/*=====================================================================
ArgumentParser.h
----------------
Copyright Glare Technologies Limited 2021 - 
=====================================================================*/
#pragma once


#include "Exception.h"
#include <vector>
#include <string>
#include <map>


class ArgumentParserExcep : public glare::Exception
{
public:
	ArgumentParserExcep(const std::string& s_) : glare::Exception(s_) {}
};


/*=====================================================================
ArgumentParser
--------------
For parsing command line arguments.

Use like this:

std::map<std::string, std::vector<ArgumentParser::ArgumentType> > syntax;
syntax["-n"] = std::vector<ArgumentParser::ArgumentType>(1, ArgumentParser::ArgumentType_string); // One string arg
syntax["--test"] = std::vector<ArgumentParser::ArgumentType>(); // Zero args
syntax["--unpack"] = std::vector<ArgumentParser::ArgumentType>(2, ArgumentParser::ArgumentType_string); // 2 string args

ArgumentParser parser(args, syntax);
=====================================================================*/
class ArgumentParser
{
public:
	enum ArgumentType
	{
		ArgumentType_string,
		ArgumentType_int,
		ArgumentType_double
	};

	class ParsedArg
	{
	public:
		ParsedArg() : double_val(-666.0), int_val(-666) {};
		~ParsedArg(){}

		std::string string_val;
		double double_val;
		int int_val;
		ArgumentType type;

		const std::string toString() const;
	};
	

	ArgumentParser(const std::vector<std::string>& args, const std::map<std::string, std::vector<ArgumentType> >& syntax); // Throws ArgumentParserExcep
	ArgumentParser();

	~ArgumentParser();

	bool isArgPresent(const std::string& name) const { return parsed_args.find(name) != parsed_args.end(); }
	
	const std::string getArgStringValue(const std::string& name, unsigned int value_index = 0) const;
	int getArgIntValue(const std::string& name, unsigned int value_index = 0) const;
	double getArgDoubleValue(const std::string& name, unsigned int value_index = 0) const;

	const std::string getUnnamedArg() const { return unnamed_arg; }
	void setUnnamedArg(const std::string& s) { unnamed_arg = s; }

	const std::vector<std::string> getArgs() const;
	const std::string getArgsAsString() const;

	void appendToOrCreateArg(const std::string& name, const std::string& value);

	void setArg(const std::string& name, const std::vector<ParsedArg>& args_) { parsed_args[name] = args_; }
	void setStringArg(const std::string& name, const std::string& s);
	void removeArg(const std::string& name);

private:
	std::map<std::string, std::vector<ArgumentType> > syntax;
	std::map<std::string, std::vector<ParsedArg> > parsed_args;
	std::string unnamed_arg;
};
