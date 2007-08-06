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
/*========================================================================
Inifile class

Coded by Nick Chapman
nickamy@paradise.net.nz
December 2000
========================================================================*/
#ifndef __INIFILE_H__
#define __INIFILE_H__

#include "platform.h"
#include <string>
#include <map>
#include "myfile.h"//my custom file class


class IniFileExcep
{
public:
	IniFileExcep(const std::string& message_) : message(message_) {}
	~IniFileExcep(){}

	const std::string& getMessage() const { return message; }
private:
	std::string message;
};


/*========================================================================
IniFile
-------
Class for getting <key, value> pair information from an .ini file.

Store the info in a text file like this:
---------start example------------

"keyname1" "valuename1"

"keyname2" "valuename2"

---------end example--------------

Whitespace is ignored outside of string quotes.
comments can be done like this:

--this is a comment

comments inside a pair of string quotes will be ignored.
So
---------start example------------

"	--this is a key--	"  "the value"

---------end example--------------
is ok.



IMPORTANT: NOT compatible with normal .ini files like in windows.

Stuff to do:
*	handle parse errors and file opening errors better.
	Exceptions?

========================================================================*/
class IniFile
{
public:
	/*========================================================================
	IniFile
	-------
	opens up the file called 'filename',
	reads in the keys + values, then closes the file.
	========================================================================*/
	IniFile(const std::string& filename) throw (IniFileExcep);

	/*========================================================================
	~IniFile
	--------
	Does pretty much nothing.
	========================================================================*/
	~IniFile();

	/*========================================================================
	reload
	------
	Deletes all the keys and values,
	opens up the file that the keys and values were last read from,
	and reads in all the keys and values again.

	Use this if the .ini file has been changed since this object was constructed,
	and you want to use the new values.
	========================================================================*/	
	bool reload();

	/*========================================================================
	reload
	------
	reload the keys and values from a specified file.
	========================================================================*/	
	bool reload(const std::string& filename);

	/*========================================================================
	getValueForKey
	--------------
	parameters:
	*	key: the key (without the quotes around it) to use

	returns:
		ref to the value found (without quotes around it), 

	throws exception if no such key found
	========================================================================*/
	const std::string& getValueForKey(const std::string& key) throw (IniFileExcep);


	int getIntForKey(const std::string& key) throw (IniFileExcep);

	float getFloatForKey(const std::string& key) throw (IniFileExcep);
	double getDoubleForKey(const std::string& key) throw (IniFileExcep);
	bool getBoolForKey(const std::string& key) throw (IniFileExcep);

	/*========================================================================
	getFileName
	-----------
	get the name of the file the keys + values were last read from.
	This is the filename used when reload() is called without a parameter being
	supplied.
	========================================================================*/
	inline const std::string& getFileName() const { return filename; }





private:
	void readFromFile();
	inline void advance()
	{
		currenttoken = file.readChar();
		debugcurrentoken = currenttoken;
	}
	void eatWhiteSpace();

	void parseError(const std::string& message);
	void parseString(std::string& string_out);
	void parseComment();

	std::string filename;

	MyFile file;
	typedef std::map<std::string, std::string> MAPTYPE;
	MAPTYPE	keymap;

	int currenttoken;
	char debugcurrentoken;//just so it will tell me which char it is
};





#endif //__INIFILE_H__
