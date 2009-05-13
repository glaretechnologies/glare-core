/*=====================================================================
ArgumentParser.h
----------------
File created by ClassTemplate on Sat Nov 03 18:21:05 2007
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __ARGUMENTPARSER_H_666_
#define __ARGUMENTPARSER_H_666_


#include <vector>
#include <string>
#include <map>

class ArgumentParserExcep
{
public:
	ArgumentParserExcep(const std::string& s_) : s(s_) {}
	~ArgumentParserExcep(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};



/*=====================================================================
ArgumentParser
--------------

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
	

	/*=====================================================================
	ArgumentParser
	--------------
	
	=====================================================================*/
	ArgumentParser(const std::vector<std::string>& args, const std::map<std::string, std::vector<ArgumentType> >& syntax);
	ArgumentParser(){}

	~ArgumentParser();


	bool isArgPresent(const std::string& name) const { return parsed_args.find(name) != parsed_args.end(); }
	
	const std::string getArgStringValue(const std::string& name, unsigned int value_index = 0) const;
	int getArgIntValue(const std::string& name, unsigned int value_index = 0) const;
	double getArgDoubleValue(const std::string& name, unsigned int value_index = 0) const;

	const std::string getUnnamedArg() const { return unnamed_arg; }
	void setUnnamedArg(const std::string& s) { unnamed_arg = s; }

	const std::vector<std::string> getArgs() const;// { return args; }
	const std::string getArgsAsString() const;


	//const std::vector<std::string> getArgsWithoutUnnamedAndZerothArg() const;

	void appendToOrCreateArg(const std::string& name, const std::string& value);

	void setArg(const std::string& name, const std::vector<ParsedArg>& args_) { parsed_args[name] = args_; }
	void setStringArg(const std::string& name, const std::string& s);
	void removeArg(const std::string& name);

	//const std::vector<std::string>& getOriginalArgs() const { return args; }
	//const std::string getOriginalArgsAsString() const;

private:
	//std::vector<std::string> args;
	std::map<std::string, std::vector<ArgumentType> > syntax;
	std::map<std::string, std::vector<ParsedArg> > parsed_args;
	std::string unnamed_arg;
};



#endif //__ARGUMENTPARSER_H_666_




