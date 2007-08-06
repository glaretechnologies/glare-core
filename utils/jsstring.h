/*=====================================================================
jsstring.h
----------
File created by ClassTemplate on Mon Oct 25 01:56:30 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __JSSTRING_H_666_
#define __JSSTRING_H_666_





/*=====================================================================
JSString
--------
NOTE: right now this class is fairly lame, use at ur own risk!!!!

custom string class.
Mainly for compatibility across DLL boundaries.

  more or less modelled on std::string.
=====================================================================*/
class JSString
{
public:
	/*=====================================================================
	JSString
	--------
	
	=====================================================================*/
	JSString();

	//explicit means do not use this constructor for implicit conversion.
	explicit JSString(const char* str);

	JSString(const JSString& other);

	~JSString();


	JSString& operator = (const JSString& other);

	bool operator == (const JSString& other) const;
	bool operator != (const JSString& other) const;

	bool operator == (const char* otherstr) const;
	bool operator != (const char* otherstr) const;

	char operator[] (int index) const { return data[index]; }


	//not counting NULL terminator.
	int size() const;


	//always returns valid (non-null) pointer.
	//string is always NULL-terminated.
	inline const char* c_str() const{ return data; }


	static void doUnitTests();

private:
	char* data;
};



#endif //__JSSTRING_H_666_




