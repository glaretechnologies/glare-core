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
	/*=====================================================================
	ArgumentParser
	--------------
	
	=====================================================================*/
	ArgumentParser(const std::vector<std::string>& args, const std::map<std::string, int>& syntax);

	~ArgumentParser();


	bool isArgPresent(const std::string& name) const { return parsed_args.find(name) != parsed_args.end(); }
	const std::string getArgValue(const std::string& name, unsigned int value_index = 0) const;
	int getArgIntValue(const std::string& name, unsigned int value_index = 0) const;
	double getArgDoubleValue(const std::string& name, unsigned int value_index = 0) const;

	const std::string getUnnamedArg() const { return unnamed_arg; }

	const std::vector<std::string>& getArgs() const { return args; }

private:
	std::vector<std::string> args;
	std::map<std::string, int> syntax;
	std::map<std::string, std::vector<std::string> > parsed_args;
	std::string unnamed_arg;
};



#endif //__ARGUMENTPARSER_H_666_




