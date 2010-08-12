/*=====================================================================
Obfuscator.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Tue May 25 18:32:39 +1200 2010
=====================================================================*/
#include "Obfuscator.h"


#include "Parser.h"
#include <string>
#include "../indigo/globals.h"
#include "../utils/stringutils.h"
#include "../utils/Exception.h"
#include "../utils/fileutils.h"
#include <fstream>


Obfuscator::Obfuscator(bool collapse_whitespace_, bool remove_comments_, bool change_tokens_, bool cryptic_tokens_)
:	collapse_whitespace(collapse_whitespace_),
	remove_comments(remove_comments_),
	change_tokens(change_tokens_),
	cryptic_tokens(cryptic_tokens_),
	rng(1)
{
	c = 0;

	const char* keywords_[] = {
"auto",
"break",
"case",
"char",
"const",
"continue",
"default",
"do",
"double",
"else",
"enum",
"extern",
"float",
"for",
"goto",
"if",
"int",
"long",
"register",
"return",
"short",
"signed",
"sizeof",
"static",
"struct",
"switch",
"typedef",
"union",
"unsigned",
"void",
"volatile",
"while",

"float4",
"uint",
"uint4",
"texture",
"__device__",
"__constant__",
"__global__",
"__shared__",
"tex1Dfetch",
"__float_as_int",

"acos",
"asin",
"atan2",
"cos",
"sin",
"tan",
"sqrt",
"min",
"max",
"rsqrt",
"pow",

// float4 elements:
"x",
"y",
"z",
"w",

NULL
	};

	for(int i=0; keywords_[i]; i++)
	{
		//conPrint(keywords_[i]);
		this->keywords.insert(keywords_[i]);
	}
}


Obfuscator::~Obfuscator()
{

}


const std::string Obfuscator::mapToken(const std::string& t)
{
	if(!change_tokens)
		return t;

	if(this->keywords.find(t) != this->keywords.end())
		return t;

	if(token_map.find(t) == token_map.end())
	{
		std::string new_token;
		if(cryptic_tokens)
		{
			while(1)
			{
				new_token = "";
				
				for(int i=0; i<10; ++i)
				{
					if(rng.unitRandom() < 0.5f)
						::concatWithChar(new_token, '1');
					else
						::concatWithChar(new_token, 'l');
				}
				if(new_tokens.find(new_token) == new_tokens.end()) // If we haven't already randomly generated this token...
				{
					new_tokens.insert(new_token);
					break;
				}
			}
		}
		else
		{
			new_token = "token_" + ::toString(c);
			c++;
		}

		token_map[t] = new_token;
	}
	else
	{
		
	}

	return token_map[t];
}

const std::string Obfuscator::obfuscate(const std::string& s)
{
	Parser p(s.c_str(), (unsigned int)s.length());

	std::string res;


	const std::string ignore_tokens = "f[](){}<>/*-+=;,.&!|?:";


	while(p.notEOF())
	{
		std::string t;

		//const char debug_current_char = p.current();
		//conPrint(std::string(1, debug_current_char));

		if(p.current() == '/' && p.nextIsChar('/'))
		{
			int last_currentpos = p.currentPos();
			p.parseLine();
			if(!remove_comments)
				res += s.substr(last_currentpos, p.currentPos() - last_currentpos); // append comment to output
			continue;
		}
		else if(p.current() == '#') // preprocessor def
		{
			int last_currentpos = p.currentPos();
			p.advancePastLine();
			if(res.substr(res.size() - 1, 1) != "\n")
				res += "\r\n";
			res += s.substr(last_currentpos, p.currentPos() - last_currentpos);
			continue;
		}
		else if(p.current() == '"') // string
		{
			int last_currentpos = p.currentPos();
			p.parseChar('"');

			while(p.notEOF() && p.current() != '"')
			{
				p.advance();
			}

			p.parseChar('"');

			res += s.substr(last_currentpos, p.currentPos() - last_currentpos);

			continue;
		}
		else if(p.current() == '/' && p.nextIsChar('*'))
		{
			int last_currentpos = p.currentPos();
			p.parseChar('/');
			p.parseChar('*');

			while(p.notEOF() && !(p.current() == '*' && p.nextIsChar('/')))
			{
				p.advance();
			}
			p.parseChar('*');
			p.parseChar('/');
			if(!remove_comments)
				res += s.substr(last_currentpos, p.currentPos() - last_currentpos); // append comment to output
			continue;
		}
		else if(::isWhitespace(p.current()))
		{
			int last_currentpos = p.currentPos();
			p.parseWhiteSpace();
			if(collapse_whitespace)
				res += " ";
			else
				res += s.substr(last_currentpos, p.currentPos() - last_currentpos);
			continue;
		}
		else if(p.parseIdentifier(t))
		{
			res += mapToken(t);
			continue;
		}
		else if(p.fractionalNumberNext())
		{
			int last_currentpos = p.currentPos();
			double val;
			p.parseDouble(val);
			res += s.substr(last_currentpos, p.currentPos() - last_currentpos);
			continue;
		}
		else if(::isNumeric(p.current()))
		{
			int last_currentpos = p.currentPos();
			int val;
			p.parseInt(val);
			res += s.substr(last_currentpos, p.currentPos() - last_currentpos);
			continue;
		}
		
		/*if(p.parseInt())
		{
			int last_currentpos = p.currentPos();
			p.parseDouble();
			res += s.substr(last_current, p.currentPos() - last_currentpos);
		}*/

		{
			bool found = false;
			for(int i=0; i<ignore_tokens.size() && !found; ++i)
			{
				if(p.parseChar(ignore_tokens[i]))
				{
					res += ignore_tokens[i];
					found = true;
				}
			}
			if(found)
				continue;
		}

		//if(!processed)
		//{
			throw Indigo::Exception("Unhandled character at start of '" + s.substr(p.currentPos(), 10) + "'");
		//}
	}

	return res;
}

#if (BUILD_TESTS)
void Obfuscator::test()
{
	//const std::string s = "int /*a = b[3] + */a;";

	std::string s;
	FileUtils::readEntireFile("c:\\programming\\indigo\\trunk\\cuda\\NewCudaTwoLevelRayTracingKernel.cu", s);

	
	Obfuscator ob(
		false, // collapse_whitespace
		false, // remove_comments
		false, // change tokens
		false // cryptic_tokens
	);

	try
	{
		const std::string ob_s = ob.obfuscate(s);

		//conPrint(ob_s);

		{
			//std::ofstream f("munged.cpp");
			//f << ob_s;
			const std::string outpath = "c:\\programming\\indigo\\trunk\\cuda\\NewCudaTwoLevelRayTracingKernel_obfuscated.cu";
			FileUtils::writeEntireFile(outpath, ob_s);
			conPrint("Written '" + outpath + "'");
		}
	}
	catch(Indigo::Exception& e)
	{
		conPrint(e.what());
	}

	
	

	//exit(0);
}
#endif