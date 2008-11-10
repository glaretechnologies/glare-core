/*=====================================================================
mystream.h
----------
File created by ClassTemplate on Fri Apr 19 12:18:01 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MYSTREAM_H_666_
#define __MYSTREAM_H_666_


//class Vec3;
#include <string>//get rid of this?


//#pragma warning (disable:4800)
//disable "forcing value to bool 'true' or 'false' (performance warning)"


class MyStreamExcep	
{
public:
	MyStreamExcep(const std::string& message_) : message(message_) {}
	~MyStreamExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};


/*=====================================================================
MyStream
--------
Generalised stream interface.
Reads and writes most basic types.
=====================================================================*/
class MyStream
{
public:
	/*=====================================================================
	MyStream
	--------
	
	=====================================================================*/
	//MyStream();

	virtual ~MyStream(){}

	//------------------------------------------------------------------------
	//writes
	//------------------------------------------------------------------------
	virtual void write(char x) = 0;
	//virtual void write(unsigned char x) = 0;
	virtual void write(unsigned short x) = 0;
	virtual void write(int x) = 0;
	virtual void write(float x) = 0;
	virtual void write(const std::string& s) = 0;//writes null-terminated string
	virtual void write(const void* data, int numbytes) = 0;
	//virtual void write(const Vec3& vec) = 0;

	inline void write(unsigned char x){ write( *(char*)&x ); }
	inline void write(unsigned int x){ write( *(int*)&x ); }//NOTE: testme
	inline void write(short x){ write( *(unsigned short*)&x ); }
	inline void write(bool x){ write( *(unsigned char*)&x ); }

	//------------------------------------------------------------------------
	//reads
	//------------------------------------------------------------------------
	virtual void readTo(char& x) = 0;
	virtual void readTo(unsigned short& x) = 0;
	virtual void readTo(float& x) = 0;
	virtual void readTo(int& x) = 0;
	virtual void readTo(std::string& x, int maxlength) = 0;
	virtual void readTo(void* buffer, int numbytes) = 0;
	//virtual void readTo(Vec3& x) = 0;

	inline void readTo(unsigned char& x);
	inline void readTo(bool& x);
	inline void readTo(short& x);
	inline void readTo(unsigned int& x);

};

//reading of types that can easily be implemented in terms of existing methods
inline void MyStream::readTo(bool& x)
{
	unsigned char y;
	readTo(y);
	x = *(bool*)&y;
}	
	
inline void MyStream::readTo(short& x)
{
	unsigned short y;
	readTo(y);
	x = *(short*)&y;
}

inline void MyStream::readTo(unsigned int& x)
{
	int y;
	readTo(y);
	x = *(unsigned int*)&y;
}

inline void MyStream::readTo(unsigned char& x)
{
	char y;
	readTo(y);
	x = *(unsigned char*)&y;
}


//------------------------------------------------------------------------
//operator << and >> for nicer looking code and a uniform syntax
//for reading and writing non-basic types.
//------------------------------------------------------------------------
inline MyStream& operator << (MyStream& stream, const std::string& s)
{
	stream.write(s);
	return stream;
}

/*
inline MyStream& operator >> (MyStream& stream, std::string& s)
{
	stream.readTo(s);
	return stream;
}*/


inline MyStream& operator << (MyStream& stream, bool x)
{
	stream.write(x);
	return stream;
}

inline MyStream& operator >> (MyStream& stream, bool& x)
{
	stream.readTo(x);
	return stream;
}


inline MyStream& operator << (MyStream& stream, char x)
{
	stream.write(x);
	return stream;
}

inline MyStream& operator >> (MyStream& stream, char& x)
{
	stream.readTo(x);
	return stream;
}

inline MyStream& operator << (MyStream& stream, unsigned char x)
{
	stream.write(x);
	return stream;
}

inline MyStream& operator >> (MyStream& stream, unsigned char& x)
{
	stream.readTo(x);
	return stream;
}

inline MyStream& operator << (MyStream& stream, short x)
{
	stream.write(x);
	return stream;
}

inline MyStream& operator >> (MyStream& stream, short& x)
{
	stream.readTo(x);
	return stream;
}

inline MyStream& operator << (MyStream& stream, unsigned short x)
{
	stream.write(x);
	return stream;
}

inline MyStream& operator >> (MyStream& stream, unsigned short& x)
{
	stream.readTo(x);
	return stream;
}

inline MyStream& operator << (MyStream& stream, int x)
{
	stream.write(x);
	return stream;
}

inline MyStream& operator >> (MyStream& stream, int& x)
{
	stream.readTo(x);
	return stream;
}

inline MyStream& operator << (MyStream& stream, unsigned int x)
{
	stream.write(x);
	return stream;
}

inline MyStream& operator >> (MyStream& stream, unsigned int& x)
{
	stream.readTo(x);
	return stream;
}

inline MyStream& operator << (MyStream& stream, float x)
{
	stream.write(x);
	return stream;
}

inline MyStream& operator >> (MyStream& stream, float& x)
{
	stream.readTo(x);
	return stream;
}

#endif //__MYSTREAM_H_666_




