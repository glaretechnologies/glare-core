/*===================================================================

  
  digital liberation front 2001
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|        \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#include "inifile.h"

//#pragma warning(disable:4786)


#include <assert.h>

#include "stringutils.h"//for charToString()
//#include <iostream>//TEMP



IniFile::IniFile(const std::string& filename_)// throw (IniFileExcep)
:	filename(filename_)//,
//	file(filename_, "r")//open the file
{




	//assert(file.isOpen());
	try
	{
		file.open(filename_, "r");
	}
	catch(FileNotFoundExcep&)
	{
		throw IniFileExcep("could not open file '" + filename_ + "'.");
	}

	//-----------------------------------------------------------------
	//read in the values from the file and store in the map
	//-----------------------------------------------------------------
	readFromFile();

	//-----------------------------------------------------------------
	//close the file, as all the values are read in
	//-----------------------------------------------------------------
	file.close();
}

IniFile::~IniFile()
{
	assert(!file.isOpen());
}


bool IniFile::reload()
{
	return reload(filename);
}


bool IniFile::reload(const std::string& newfilename)
{
	//-----------------------------------------------------------------
	//clear out the map
	//-----------------------------------------------------------------
	keymap.clear();

	assert(!file.isOpen());

	//-----------------------------------------------------------------
	//open the new file
	//-----------------------------------------------------------------
	file.open(newfilename, "r");

	assert(file.isOpen());

	//-----------------------------------------------------------------
	//read contents of the file
	//-----------------------------------------------------------------
	readFromFile();

//	std::cout << "Finished reading from file..." << std::endl;//TEMP


	//-----------------------------------------------------------------
	//close the file
	//-----------------------------------------------------------------
	file.close();

	return true;
}





const std::string& IniFile::getValueForKey(const std::string& key) //throw (IniFileExcep)
{
	//-----------------------------------------------------------------
	//lookup the key in the map
	//-----------------------------------------------------------------
	MAPTYPE::iterator result = keymap.find(key);

	//-----------------------------------------------------------------
	//lookup failed if we got end()
	//-----------------------------------------------------------------
	if(result == keymap.end())
	{
		throw IniFileExcep("Lookup failed: key '" + key + "' is not in the map");
	}

	return (*result).second;
}


int IniFile::getIntForKey(const std::string& key) //throw (IniFileExcep)
{
	return stringToInt(getValueForKey(key));
}


float IniFile::getFloatForKey(const std::string& key) //throw (IniFileExcep)
{
	return stringToFloat(getValueForKey(key));
}

double IniFile::getDoubleForKey(const std::string& key)// throw (IniFileExcep)
{
	return stringToDouble(getValueForKey(key));
}

bool IniFile::getBoolForKey(const std::string& key) //throw (IniFileExcep)
{
	const std::string val = getValueForKey(key);
	if(::toLowerCase(val) == "true")
		return true;
	else if(::toLowerCase(val) == "false")
		return false;
	else
		throw IniFileExcep("boolean value for key '" + key + "' must be true or false.");
}




void IniFile::readFromFile()
{
	assert(file.isOpen());

	advance();//read first token in file

	bool breakwhileloop = false;
	while(!breakwhileloop)
	{
//		std::cout << "Looping..." << std::endl;//TEMP

		switch(currenttoken)
		{
		case '"':
			{
//				std::cout << "Reading in key/value pair..." << std::endl;//TEMP

				//-----------------------------------------------------------------
				//read in key string
				//-----------------------------------------------------------------
				std::string key;
				parseString(key);

				eatWhiteSpace();

				//-----------------------------------------------------------------
				//read in value string
				//-----------------------------------------------------------------
				std::string value;
				parseString(value);

//				std::cout << "(key, value): (" << key << ", " << value<< ")" << std::endl;//TEMP

				//-----------------------------------------------------------------
				//store the value in the map
				//-----------------------------------------------------------------
				keymap[key] = value;//.insert(key, value);
				break;
			}
		case EOF:
			{
				breakwhileloop = true;
				break;
			}
		case '-':
			{
//				std::cout << "Parsing comment..." << std::endl;//TEMP

				parseComment();
				break;
			}
		default:
			{
//				std::cout << "default case..." << std::endl;//TEMP

				if(isWhitespace(currenttoken))
				{
					advance();
				}
				else
				{
					parseError("unknown symbol: '" + toString((char)currenttoken) + "'");
					breakwhileloop = true;
					advance();
				}
			}
		}
	}

	return;
	
}


void IniFile::parseError(const std::string& message)
{
	throw IniFileExcep("Parse Error: " + message + "\n");
}
	//"PARSE ERROR: 


void IniFile::parseString(std::string& string_out)
{
	//assert(currenttoken == '"');
	if(currenttoken != '"')
	{
		parseError("next token after '\"' was not '\"'"); 
		return;
	}

	assert(string_out.size() == 0);

	advance();

	bool breakwhileloop = false;
	while(!breakwhileloop)
	{
		switch(currenttoken)
		{
		case '"':
			{
				breakwhileloop = true;
				advance();
				break;
			}
		case EOF:
			{
				parseError("opening quote without closing quote");
				return;
			}
		default:
			{
				string_out += currenttoken;
				advance();

			}
		}

	}

}




void IniFile::parseComment()
{
	assert(currenttoken == '-');

	advance();

	bool breakwhileloop = false;
	while(!breakwhileloop)
	{
		switch(currenttoken)
		{
		case '\n':
			{
				breakwhileloop = true;
				advance();
				break;
			}
		case EOF:
			{
				return;
			}
		default:
			{
				advance();
			}
		}
	}
}

void IniFile::eatWhiteSpace()
{
	while(isWhitespace(currenttoken))
		advance();
}

	/*while(1)
	{
		if(isWhiteSpace(currenttoken))
		{
			advance();
		}
		else
		{
			return;
		}
	}*/

